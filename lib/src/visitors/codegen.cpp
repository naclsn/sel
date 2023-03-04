#ifdef LLVM_CODEGEN

#include <fstream>

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>

#include "sel/builtins.hpp"
#include "sel/visitors.hpp"
#include "sel/parser.hpp"

using namespace llvm;

#include "_templlate.hpp"

using namespace tll;

namespace sel {

  // things that can come out of SyGen, passed to the injection `also(brk, cont, <>)`
  struct Generated {
    struct let_ptr_len { let<char const*> ptr; let<int> len; };

    union {
      let<double> _num;
      let_ptr_len _buf;
    };
    enum { NUM, BUF } tag;

    Generated(let<double> num): _num(num), tag(NUM) { }
    Generated(let<char const*> ptr, let<int> len): _buf{ptr, len}, tag(BUF) { }
    Generated(let_ptr_len ptr_len): _buf(ptr_len), tag(BUF) { }

    inline let<double> const& num() const {
      if (NUM != tag) throw CodegenError("expected symbol to generate a number");
      return _num;
    }
    inline let_ptr_len const& buf() const {
      if (BUF != tag) throw CodegenError("expected symbol to generate a buffer");
      return _buf;
    }

    Generated(Generated const& other): tag(other.tag) {
      switch (tag) {
        case NUM:
          new(&_num) auto(other._num);
          break;
        case BUF:
          new(&_buf.ptr) auto(other._buf.ptr);
          new(&_buf.len) auto(other._buf.len);
          break;
      }
    }
    ~Generated() {
      switch (tag) {
        case NUM:
          _num.~let<double>();
          break;
        case BUF:
          _buf.ptr.~let<char const*>();
          _buf.len.~let<int>();
          break;
      }
    }
  };


  // note that the `body` function gets the bytes pointed at,
  // not the iteration variable (ie a chr, not chr* nor len)
  void makeBufferLoop(std::string const name
    , Generated::let_ptr_len ptr_len
    , std::function<void(BasicBlock* brk, BasicBlock* cont, let<char> at)> body
    )
  {
    // {before} -> body -> incr -> after
    //      \       | ^----'       ^
    //       `------+-------------'

    auto* before_bb = here();
    auto* body_bb = block(name+"_loop_body");
    auto* incr_bb = block(name+"_loop_incr");
    auto* after_bb = block(name+"_loop_break");

    // before -> {body}    after
    //     `----------------^
    cond(ptr_len.len > 0, body_bb, after_bb);

    point(body_bb);
    // when first entering, init to 0
    letPHI<int> k;
    k.incoming(0, before_bb);

    // actual loop body
    // body -> incr   after
    //  `-------------^
    body(after_bb, incr_bb, ptr_len.ptr[k]);

    point(incr_bb);
    // when from continue, take the ++
    auto kpp = k + 1;
    k.incoming(kpp, incr_bb);

    // body    incr -> {after}
    //    ^----'
    cond(ptr_len.len == kpp, after_bb, body_bb);
    point(after_bb);
  }

  void makeStream(std::string const name
    , std::function<void(BasicBlock* brk, BasicBlock* cont)> chk
    , std::function<Generated()> itr
    , std::function<void(BasicBlock* brk, BasicBlock* cont, Generated it)> also
    )
  {
    // entry -> check -> [iter -> inject] (inject must branch to exit or check)
    //           |  ^-------------' v
    //           `-------------> exit (insert point ends up in exit, which is empty)

    // entry -> {check}
    auto* check = block(name+"_check");
    br(check);
    point(check);

    auto* exit = block(name+"_exit");
    auto* iter = block(name+"_iter");
    // (responsibility of chk)
    // {check}  -> [iter
    //    `--------> exit
    chk(exit, iter);

    // [{iter}
    point(iter);
    auto ptr_len = itr();

    // [iter -> {inject}]
    auto* inject = block(name+"_inject");
    br(inject);
    point(inject);
    // (responsibility of also)
    // check -> [iter -> inject]
    //  |  ^-------------' v
    //  `-------------> exit
    also(exit, check, ptr_len);

    // {exit}
    point(exit);
  }


  /// the injection (sometimes `also`) must branch to exit or iter
  /// in the case of a SyNum, brk and cont are both the same
  typedef std::function
    < void
      (llvm::BasicBlock* brk, llvm::BasicBlock* cont, Generated it)
    > inject_clo_type;
  typedef std::function<void(Codegen&, inject_clo_type)> clo_type;

  struct Symbol {
    std::string name;
    clo_type doobidoo;

    Symbol(std::string const name, clo_type doobidoo)
      : name(name)
      , doobidoo(doobidoo)
    { }
    virtual ~Symbol() { }

    virtual void make(Codegen& cg, inject_clo_type also) const { doobidoo(cg, also); }
  };

  typedef void (* Head)(Codegen&, inject_clo_type);

  struct Codegen {
    VisCodegen& vis;
    indented& log;
    std::stack<Symbol, std::list<Symbol>> systack;

    Codegen(VisCodegen& vis, indented& log): vis(vis), log(log) { }

    void place(Symbol sy);
    Symbol const take();

    void invoke(Val const&);

    static std::unordered_map<std::string, Head> head_impls;
  };

  // excluding the Head, each part that are `{arg, base}` closures
  struct NotHead {
    Val const& arg;
    Val const& base;
    NotHead(Val const& arg, Val const& base)
      : arg(arg)
      , base(base)
    { }
    void operator()(Codegen& cg, inject_clo_type also) {
      cg.invoke(arg);
      cg.invoke(base);
      cg.take().make(cg, also);
    }
  };

  void Codegen::place(Symbol sy) {
    log << "+ " << sy.name << "\n";
    systack.push(sy);
  }

  Symbol const Codegen::take() {
    if (systack.empty()) throw CodegenError("trying to take from empty symbol stack");
    Symbol r = systack.top();
    log << "- " << r.name << "\n";
    systack.pop();
    return r;
  }

  void Codegen::invoke(Val const& arg) {
      size_t before = systack.size();

      log << "invoke:\n" << repr(arg, false) << "\n";
      arg.accept(vis);

      int change = systack.size() - before;
      if (1 != change) {
        std::ostringstream oss("expected 1 more symbols on stack but got ", std::ios::ate);

        if (0 == change) oss << "no change";
        else oss << std::abs(change) << (0 < change ? " more" : " fewer");

        throw CodegenError(oss.str());
      }
  }


  void VisCodegen::visitCommon(Segment const& it, char const* name) {
    cg.place({name, NotHead(it.arg(), it.base())});
  }

  void VisCodegen::visitCommon(Val const& it, char const* name) {
    auto iter = cg.head_impls.find(name);
    if (cg.head_impls.end() == iter) {
      std::ostringstream oss;
      throw NIYError((oss << "codegen for " << quoted(name), oss.str()));
    }
    cg.place({name, iter->second});
  }

  // `i1 get(i8**, i32*)` and `put(i8*, i32)`
  typedef let<void(bool(char const**, int*), void(char const*, int))> app_t;

  VisCodegen::VisCodegen(char const* file_name, char const* module_name, char const* function_name, App& app)
    : context()
    , builder(context)
    , module(module_name, context)
    , log(*new indented("   ", std::cerr))
    , cg(*new Codegen(*this, log))
    , funname(function_name)
  {
    tll::builder(builder);

    module.setSourceFileName(file_name);

    Val& f = app.get(); // ZZZ: temporary dum accessor
    // type-check (for now it only can do `Str* -> Str*`)
    {
      Type const& ty = f.type();
      if (Ty::FUN != ty.base()) {
        std::ostringstream oss;
        throw TypeError((oss << "value of type '" << ty << "' is not a function", oss.str()));
      }
      // (with special case for eg. id_/const_, hacked in for dev)
      bool from_ok = Ty::STR == ty.from().base() || Ty::UNK == ty.from().base();
      bool to_ok = Ty::STR != ty.to().base() || Ty::UNK != ty.to().base();
      if (!from_ok || !to_ok) {
        std::ostringstream oss;
        throw NIYError((oss << "compiling an application of type '" << ty << "', only 'Str* -> Str*' is supported", oss.str()));
      }
    }

    app_t app_f(funname, module, Function::ExternalLinkage);
    point(app_f.entry());

    // place input symbol (dont like having to construct a `Input` for this)
    std::stringstream zin;
    visit(*new Input(app, zin));

    // build the whole stack
    // coerse<Val>(app, new Input(app, in), ty.from()); // TODO: input coersion (Str* -> a)
    cg.invoke(f);
    // coerse<Str>(app, (*(Fun*)f)(), Type(Ty::STR, {0}, TyFlag::IS_INF)); // TODO: output coersion (b -> Str*)

    auto put = tll::get<1>(app_f);

    // start taking from the stack and generate the output
    cg.take().make(cg, [&put](BasicBlock* brk, BasicBlock* cont, Generated it) {
      (*put)(it.buf().ptr, it.buf().len);
      br(cont);
    });

    app_f.ret();

    if (!cg.systack.empty()) throw CodegenError("symbol stack should be empty at the end");
  }

  VisCodegen::~VisCodegen() {
    delete &log;
    delete &cg;
  }

  void VisCodegen::makeMain() {
    // reclaim existing function
    app_t app_f(funname, module);

    log << "generating a `main` entry point\n";
    let<int()> main("main", module, Function::ExternalLinkage);

    let<bool(char const**, int*)> get("get", module, Function::PrivateLinkage);
    let<void(char const*, int)> put("put", module, Function::PrivateLinkage);

    point(main.entry()); {
      app_f(*get, *put);
      main.ret(0);
    }

    point(get.entry()); {
      auto b = tll::get<0>(get);
      auto l = tll::get<1>(get);

      // uses a static 1-char buffer for now
      letGlobal<char**> buf("buf", module, GlobalValue::PrivateLinkage, " ", false); // don't add null

      let<int()> getchar("getchar", module, Function::ExternalLinkage);
      auto r = getchar();
      auto* notend = block("notend");
      auto* end = block("end");
      cond(r == -1, end, notend);

      point(notend); {
        **buf = r.into<char>();
        *b = *buf;
        *l = 1;
        get.ret(true);
      }

      point(end); {
        *l = 0;
        get.ret(false);
      }
    }

    point(put.entry()); {
      auto b = tll::get<0>(put);
      auto l = tll::get<1>(put);

      let<int(int)> putchar("putchar", module, Function::ExternalLinkage);
      makeBufferLoop("", {b, l}, [&putchar](BasicBlock* brk, BasicBlock* cont, let<char> at) {
        putchar(at.into<int>());
        br(cont);
      });

      put.ret();
    }
  }

  void VisCodegen::compile(char const* outfile, bool link) {
    auto triple = sys::getDefaultTargetTriple();

    InitializeAllTargetInfos();
    InitializeAllTargets();
    InitializeAllTargetMCs();
    InitializeAllAsmParsers();
    InitializeAllAsmPrinters();

    std::string errmsg;
    auto* target = TargetRegistry::lookupTarget(triple, errmsg);
    if (nullptr == target) throw CodegenError(errmsg);

    char const* cpu = "generic";
    char const* features = "";

    TargetOptions opts;
    auto* machine = target->createTargetMachine(triple, cpu, features, opts, Optional<Reloc::Model>());

    module.setDataLayout(machine->createDataLayout());
    module.setTargetTriple(triple);

    std::error_code errcode;
    raw_fd_ostream dest(outfile, errcode);
    if (errcode) throw CodegenError(errcode.message());

    legacy::PassManager pass;
    // add a single emit pass
    if (machine->addPassesToEmitFile(pass, dest, nullptr, TargetMachine::CodeGenFileType::CGFT_ObjectFile))
      throw CodegenError("can't emit a file of this type");

    log << "emitting object code\n";
    pass.run(module);
    dest.flush();

    // XXX: how can I do that with the llvm c++ api, pwease
    // $ clang hello-world.o -o hello-world
    if (link && std::system(NULL)) {
      log << "linking object with libc\n";

      std::string objfile = std::string(outfile) + ".o";
      std::rename(outfile, objfile.c_str());

      std::string com = std::string("clang ") + objfile + " -o " + outfile;
      log << "`" << com << "`\n";
      int r = std::system(com.c_str());

      std::remove(objfile.c_str());

      if (0 != r) throw CodegenError("linker failed (maybe, not sure); exit status: " + std::to_string(r));
    }
  }


  void VisCodegen::visit(NumLiteral const& it) {
    double n = it.underlying();
    cg.place({std::to_string(n), [n](Codegen&, inject_clo_type also) {
      auto* after = block(std::to_string(n)+"_after");
      also(after, after, Generated(n));
      point(after);
    }});
  }

  void VisCodegen::visit(StrLiteral const& it) {
    std::string const& s = it.underlying();
    int len = s.length();

    std::string name = len < 16 ? s : ""; // unnamed if too long

    letGlobal<char const**> buffer(name, module, GlobalValue::PrivateLinkage, s, false); // don't add null

    cg.place({s, [len, name, buffer](Codegen& cg, inject_clo_type also) {
      auto* after = block(name+"_after");
      also(after, after, Generated(*buffer, len));
      point(after);
    }});
  }

  void VisCodegen::visit(LstLiteral const& it) {
    // std::vector<Val*> const& v = it.underlying();
    throw NIYError("generation of list literals");
  }

  void VisCodegen::visit(FunChain const& it) {
    std::vector<Fun*> const& f = it.underlying();
    cg.place({"chain_of_" + std::to_string(f.size()), [f](Codegen& cg, inject_clo_type also) {
      size_t k = 0;
      for (auto const& it : f) {
        cg.invoke(*it);
        auto const curr = cg.take();
        cg.place({"chain_f_" + std::to_string(++k), [curr](Codegen& cg, inject_clo_type also) {
          curr.make(cg, also);
        }});
      }
      cg.take().make(cg, also);
    }});
  }

  void VisCodegen::visit(NumDefine const& it) {
    // throw NIYError("generation off-main");
    it.underlying().accept(*this);
  }

  void VisCodegen::visit(StrDefine const& it) {
    // throw NIYError("generation off-main");
    it.underlying().accept(*this);
  }

  void VisCodegen::visit(LstDefine const& it) {
    // throw NIYError("generation off-main");
    it.underlying().accept(*this);
  }

  void VisCodegen::visit(FunDefine const& it) {
    // throw NIYError("generation off-main");
    it.underlying().accept(*this);
  }

  void VisCodegen::visit(Input const& it) {
    // reclaim existing function
    app_t app_f(funname, module);

    cg.place({"input", [app_f](Codegen&, inject_clo_type also) {
      auto get = tll::get<0>(app_f);

      auto buf = let<char const*>::alloc();
      auto len = let<int>::alloc();
      auto partial = let<bool>::alloc();
      *partial = true;

      makeStream("input",
        [&get, &buf, &len, &partial](BasicBlock* brk, BasicBlock* cont) {
          auto* read = block("read");
          // last `get` returned false (ie. not partial -> nothing more to get)
          cond(*partial, read, brk);

          point(read);
          *partial = (*get)(buf, len);

          // YYY: the condition `0 < *len` is checked by the `makeBufferLoop`
          br(cont);
        },
        [&buf, &len] {
          return Generated(*buf, *len);
        },
        also
      );
    }});
  }

  void VisCodegen::visit(StrChunks const& it) {
    std::vector<std::string> const& vs = it.chunks();
    int count = vs.size();

    std::vector<char const*> vc;
    std::vector<int> lens;
    vc.reserve(count);
    lens.reserve(count);
    for (auto const& it : vs) {
      vc.push_back(it.c_str());
      lens.push_back(it.size());
    }

    letGlobal<char const***> arr("name", module, GlobalValue::PrivateLinkage, vc);
    letGlobal<int const**> arrlens("name", module, GlobalValue::PrivateLinkage, lens);

    cg.place({"chunks", [count, lens, arr, arrlens](Codegen&, inject_clo_type also) {
      if (0 == count) {
        // noop

      } else if (1 == count) {
        // no need for iterating
        auto* after = block("chunks_of_"+std::to_string(count)+"_after");
        also(after, after, Generated(*arr[0], lens[0]));
        point(after);

      } else {
        auto* brk = block("chunks_of_"+std::to_string(count)+"_brk");
        auto* cont = block("chunks_of_"+std::to_string(count)+"_cont");

        auto* before = here();
        auto* entry = block("chunks_of_"+std::to_string(count)+"_after");
        br(entry);

        letPHI<int> k;
        k.incoming(0, before);

        // before -> entry -> brk
        //              `-> cont
        also(brk, cont, Generated(*arr[k], *arrlens[k]));

        point(cont);
          auto kpp = k+1;
          k.incoming(kpp, cont);
          br(entry);

        point(brk);
      }
    }});
  }

  void VisCodegen::visit(LstMapCoerse const& it) {
    // Lst const& l = it.source();
    throw NIYError("coersion of lists");
  }


  std::unordered_map<std::string, Head> Codegen::head_impls = {

    {"abs", [](Codegen& cg, inject_clo_type also) {
      auto const n = cg.take();

      n.make(cg, [&also](BasicBlock* brk, BasicBlock* cont, Generated it) {
        auto* before = here();
        auto* negate = block("abs_negate");
        auto* after = block("abs_after");

        auto in = it.num();
        cond(in < 0, negate, after);

        point(negate);
          auto pos = -in;
          br(after);

        point(after);
          letPHI<double> out({
            {in, before},
            {pos, negate},
          });

        also(cont, cont, out);
      });
    }},

    {"add", [](Codegen& cg, inject_clo_type also) {
      auto const a = cg.take();
      auto const b = cg.take();
      a.make(cg, [&cg, &also, &b](BasicBlock *brk, BasicBlock *cont, Generated a) {
        b.make(cg, [&also, &a](BasicBlock *brk, BasicBlock *cont, Generated b) {
          also(cont, cont, a.num() + b.num());
        });
        br(cont);
      });
    }},

    {"bytes", [](Codegen& cg, inject_clo_type also) {
      auto const s = cg.take();
      s.make(cg, [&also, &s](BasicBlock* brk, BasicBlock* cont, Generated it) {
        makeBufferLoop("bytes's " + s.name, it.buf(), [&also](BasicBlock* brk, BasicBlock* cont, let<char> at) {
          also(brk, cont, at.into<double>());
        });
        br(cont);
      });
    }},

    {"codepoints", [](Codegen& cg, inject_clo_type also) {
      auto const s = cg.take();

      auto acc = let<int>::alloc();
      auto state = let<int>::alloc();
      *state = 0;

      s.make(cg, [&also, &s, &acc, &state](BasicBlock* brk, BasicBlock* cont, Generated it) {
        makeBufferLoop("codepoints's " + s.name, it.buf(), [&also, &acc, &state](BasicBlock* brk, BasicBlock* cont, let<char> at) {
          auto* before = here();
          auto* also_bb = block("codepoints_also");

          point(also_bb);
            letPHI<double> send;

          point(before);
            auto curr = let<int>(*state);

            auto* curr_zero = block("codepoints_curr_zero");
            auto* curr_nonzero = block("codepoints_curr_nonzero");
          cond(curr == 0, curr_zero, curr_nonzero);

          point(curr_zero); {
            auto* at_ascii = block("codepoints_at_ascii");
            auto* at_nonascii = block("codepoints_at_nonascii");
            cond((at & 0b10000000) == 0, at_ascii, at_nonascii);

            point(at_ascii); {
              // ASCII-compatible
              send.incoming(at.into<double>(), at_ascii);
              br(also_bb);
            }
            point(at_nonascii); {
              // start state counting
              auto* s1 = block("codepoints_start_1");
              auto* nots1 = block("codepoints_not_start_1");
              auto* s2 = block("codepoints_start_2");
              auto* nots2 = block("codepoints_not_start_2");
              auto* s3 = block("codepoints_start_3");
              auto* nots3 = block("codepoints_not_start_3");
              cond((at & 0b00100000) == 0, s1, nots1);
              point(nots1);
              cond((at & 0b00010000) == 0, s2, nots2);
              point(nots2);
              cond((at & 0b00001000) == 0, s3, nots3);
              point(nots3); {
                send.incoming(at.into<double>(), nots3);
                br(also_bb);
              }

              point(s1); {
                // 110 -> 1 more bytes
                *acc = (at & 0b00011111).into<int>() << 6;
                *state = 1;
                br(cont);
              }
              point(s2); {
                // 1110 -> 2 more bytes
                *acc = (at & 0b00001111).into<int>() << 12;
                *state = 2;
                br(cont);
              }
              point(s3); {
                // 11110 -> 3 more bytes
                *acc = (at & 0b00000111).into<int>() << 18;
                *state = 3;
                br(cont);
              }
            } // ascii/nonascii
          }
          point(curr_nonzero); {
            auto next = curr-1;
            *state = next;

            auto upacc = let<int>(*acc) | (at & 0b00111111).into<int>();

            auto* next_zero = block("codepoints_next_zero");
            auto* next_nonzero = block("codepoints_next_nonzero");
            cond(next == 0, next_zero, next_nonzero);

            point(next_zero); {
              send.incoming(upacc.into<double>(), next_zero);
              br(also_bb);
            }
            point(next_nonzero); {
              auto shft = next*6;
              *acc = upacc << shft;
              br(cont);
            }
          } // zero/nonzero

          point(also_bb);
            also(brk, cont, send);
        });
        br(cont);
      });
    }},

    {"const", [](Codegen& cg, inject_clo_type also) {
      cg.take().make(cg, also);
      cg.take();
    }},

    {"filter", [](Codegen& cg, inject_clo_type also) {
      auto const p = cg.take();
      auto const l = cg.take();
      l.make(cg, [&cg, &also, &p](BasicBlock* brk, BasicBlock* cont, Generated it) {
        cg.place({"filter's it", [&brk, &cont, it](Codegen&, inject_clo_type also) {
          also(brk, cont, it);
        }});
        p.make(cg, [&also, &it](BasicBlock* brk, BasicBlock* cont, Generated pred_res) {
          auto is = pred_res.num() != 0;

          // if (is) {
          auto* yes = block("filter_yes");
          cond(is, yes, cont);

          point(yes);
          also(brk, cont, it);
          // }

          br(cont);
        });
        br(cont);
      });
    }},

    {"flip", [](Codegen& cg, inject_clo_type also) {
      auto const f = cg.take();
      auto const b = cg.take();
      auto const a = cg.take();
      cg.place(b);
      cg.place(a);
      // f(a, b) -> c
      f.make(cg, also);
    }},

    {"id", [](Codegen& cg, inject_clo_type also) {
      cg.take().make(cg, also);
    }},

    {"map", [](Codegen& cg, inject_clo_type also) {
      auto const f = cg.take();
      auto const l = cg.take();
      l.make(cg, [&cg, &also, &f](BasicBlock* brk, BasicBlock* cont, Generated it) {
        cg.place({"map's it", [&brk, &cont, it](Codegen&, inject_clo_type also) {
          also(brk, cont, it);
        }});
        f.make(cg, also);
        br(cont);
      });
    }},

    {"sub", [](Codegen& cg, inject_clo_type also) {
      auto const a = cg.take();
      auto const b = cg.take();
      a.make(cg, [&cg, &also, &b](BasicBlock* brk, BasicBlock* cont, Generated a) {
        b.make(cg, [&also, &a](BasicBlock* brk, BasicBlock* cont, Generated b) {
          also(cont, cont, a.num() - b.num());
        });
        br(cont);
      });
    }},

    // not perfect (eg '1-' is parsed as -1), but will do for now
    {"tonum", [](Codegen& cg, inject_clo_type also) {
      auto const s = cg.take();

      auto acc = let<double>::alloc();
      *acc = 0;

      auto isminus = let<bool>::alloc();
      *isminus = false;
      // auto isdot = let<bool>::alloc();
      // *isdot = false;

      s.make(cg, [&acc, &isminus](BasicBlock* brk, BasicBlock* cont, Generated it) {
        makeBufferLoop("tonum", it.buf(), [&acc, &isminus](BasicBlock* brk, BasicBlock* cont, let<char> at) {
          // _ -> setnegate -> after
          // `-> accumulate ---^
          auto* setnegate = block("tonum_setnegate");
          auto* accumulate = block("tonum_accumulate");
          auto* after = block("tonum_after");

          auto isnegate = at == '-' && !*isminus;
          cond(isnegate, setnegate, accumulate);

          point(setnegate);
            *isminus = true;
            br(after);

          point(accumulate);
            auto* acc_if_isdigit = block("tonum_acc_if_isdigit");
            // break if no longer digit
            cond(at >= '0' && at <= '9', acc_if_isdigit, brk);

            point(acc_if_isdigit);
              *acc = let<double>(*acc)*10 + (at-'0').into<double>();

            br(after);

          point(after);

          // continue buffer loop
          br(cont);
        });

        // continue generator loop
        br(cont);
      });

      auto res = let<double>(*acc);

      auto* nonegate = here();
      auto* negate = block("tonum_negate");
      auto* retres = block("tonum_retres");

      cond(*isminus, negate, retres);

      point(negate);
        auto nres = -res;
        br(retres);

      point(retres);
        letPHI<double> final_res({
          {nres, negate},
          {res, nonegate},
        });

      auto* after = block("tonum_inject");
      also(after, after, final_res);
      point(after);
    }},

    {"tostr", [](Codegen& cg, inject_clo_type also) {
      auto const n = cg.take();

      n.make(cg, [&cg, &also](BasicBlock* brk, BasicBlock* cont, Generated it) {
        cg.log << "have @tostr generated\n";

        auto b_1char = let<char>::alloc(2);
        auto at_2nd_char = b_1char + 1;

        auto num = it.num();

        cg.log << "call to @tostr, should only be here once\n";
        /*b=*/

        // nominus -> minus -> common
        //     `-----------------^
        auto* nominus = here();
        auto* minus = block("tostr_minus");
        auto* common = block("tostr_common");

        cond(num < 0., minus, common);

        point(minus);
          auto pnum = -num;
          *b_1char = '-';
          br(common);

        point(common);
          letPHI<double> positive({
            {pnum, minus},
            {num, nominus},
          });

          letPHI<char*> store_at({
            {at_2nd_char, minus},
            {b_1char, nominus},
          });

          letPHI<int> total_len({
            {2, minus},
            {1, nominus},
          });

          // also for now 1-char buffer, we only do digits here
          *store_at = positive.into<char>() + '0';

        also(brk, cont, Generated(b_1char, total_len));
      });
    }},

    {"unbytes", [](Codegen& cg, inject_clo_type also) {
      auto const l = cg.take();
      auto b = let<char>::alloc();
      l.make(cg, [&also, &b](BasicBlock* brk, BasicBlock* cont, Generated it) {
        *b = it.num().into<char>();
        also(brk, cont, Generated(b, 1));
      });
    }},

  };

} // namespace sel

#endif
