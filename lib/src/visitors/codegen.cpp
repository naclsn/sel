#ifdef LLVM_CODEGEN

#include <fstream>

#include "sel/visitors.hpp"
#include "sel/parser.hpp"

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>

using namespace llvm;

#include "_templlate.hpp"

using namespace tll;

namespace sel {

  struct VisCodegen::Generated {
    union {
      let<double> _num;
      struct { let<char const*> ptr; let<int> len; } _buf;
    };
    enum { NUM, BUF } tag;

    Generated(let<double> num): _num(num), tag(NUM) { }
    Generated(let<char const*> ptr, let<int> len): _buf{ptr, len}, tag(BUF) { }

    inline let<double> const& num() const {
      if (NUM != tag) throw CodegenError("expected symbol to generate a number");
      return _num;
    }
    inline decltype(_buf) const& buf() const {
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

  // structure for 'segments' of a bin
  // that is excluding the Head, each part
  // that are {arg, base} closures
  struct VisCodegen::NotHead {
    VisCodegen& cg;
    Val const& arg;
    Val const& base;
    NotHead(VisCodegen& cg, Val const& arg, Val const& base)
      : cg(cg)
      , arg(arg)
      , base(base)
    { }
    void operator()(inject_clo_type also) {
      cg.invoke(arg);
      cg.invoke(base);
      cg.take().make(also);
    }
  };


  void VisCodegen::makeBufferLoop(std::string const name
    , let<char const*> ptr
    , let<int> len
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
    cond(len > 0, body_bb, after_bb);

    point(body_bb);
    // when first entering, init to 0
    letPHI<int> k;
    k.incoming(0, before_bb);

    // actual loop body
    // body -> incr   after
    //  `-------------^
    body(after_bb, incr_bb, ptr[k]);

    point(incr_bb);
    // when from continue, take the ++
    auto kpp = k + 1;
    k.incoming(kpp, incr_bb);

    // body    incr -> {after}
    //    ^----'
    cond(len == kpp, after_bb, body_bb);
    point(after_bb);
  }

  void VisCodegen::makeStream(std::string const name
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

  void VisCodegen::place(std::string const name, clo_type doobidoo) {
    log << "+ " << name << "\n";
    systack.push(Symbol(name, doobidoo));
  }

  VisCodegen::Symbol const VisCodegen::take() {
    if (systack.empty()) throw CodegenError("trying to take from empty symbol stack");
    Symbol r = systack.top();
    log << "- " << r.name << "\n";
    systack.pop();
    return r;
  }

  void VisCodegen::invoke(Val const& arg) {
      size_t before = systack.size();

      log << "invoke:\n" << repr(arg, false) << "\n";
      this->operator()(arg);

      int change = systack.size() - before;
      if (1 != change) {
        std::ostringstream oss("expected 1 more symbols on stack but got ", std::ios::ate);

        if (0 == change) oss << "no change";
        else oss << std::abs(change) << (0 < change ? " more" : " fewer");

        throw CodegenError(oss.str());
      }
  }


  // `i1 get(i8**, i32*)` and `put(i8*, i32)`
  typedef let<void(bool(char const**, int*), void(char const*, int))> app_t;

  VisCodegen::VisCodegen(char const* file_name, char const* module_name, char const* function_name, App& app)
    : context()
    , builder(context)
    , module(module_name, context)
    , log(*new indented("   ", std::cerr))
    , funname(function_name)
  {
    tll::builder(builder);

    module.setSourceFileName(file_name);

    Val& f = app.get(); // ZZZ: temporary dum accessor
    // type-check (for now it only can do `Str* -> Str*`)
    {
      Type const& ty = f.type();
      if (Ty::FUN != ty.base) {
        std::ostringstream oss;
        throw TypeError((oss << "value of type " << ty << " is not a function", oss.str()));
      }
      if (Ty::STR != ty.from().base || Ty::STR != ty.to().base) {
        bool still_ok = false; // special case for eg. id_, hacked in for dev
        if (Ty::UNK == ty.from().base && Ty::UNK == ty.to().base)
          if (*ty.from().p.name == *ty.to().p.name)
            still_ok = true;
        if (!still_ok) {
          std::ostringstream oss;
          throw NIYError((oss << "compiling an application of type '" << ty << "', only 'Str* -> Str*' is supported", oss.str()));
        }
      }
    }

    app_t app_f(funname, module, Function::ExternalLinkage);
    point(app_f.entry());

    // place input symbol
    visitInput(Type());

    // build the whole stack
    // coerse<Val>(app, new Input(app, in), ty.from()); // TODO: input coersion (Str* -> a)
    invoke(f);
    // coerse<Str>(app, (*(Fun*)f)(), Type(Ty::STR, {0}, TyFlag::IS_INF)); // TODO: output coersion (b -> Str*)

    auto put = tll::get<1>(app_f);

    // start taking from the stack and generate the output
    take().make([&put](BasicBlock* brk, BasicBlock* cont, Generated it) {
      (*put)(it.buf().ptr, it.buf().len);
      br(cont);
    });

    app_f.ret();

    if (!systack.empty()) throw CodegenError("symbol stack should be empty at the end");
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
      makeBufferLoop("", b, l, [&putchar](BasicBlock* brk, BasicBlock* cont, let<char> at) {
        putchar(at.into<int>());
        br(cont);
      });

      put.ret();
    }
  }


  // temporary dothething
  void VisCodegen::compile(char const* outfile, bool link) {
    std::string const n = outfile;

    {
      std::ofstream ll(n + ".ll");
      if (!ll.is_open()) throw BaseError("could not open file");
      print(ll);
      ll.flush();
      ll.close();
    }

    // $ llc -filetype=obj hello-world.ll -o hello-world.o
    {
      std::ostringstream oss;
      int r = std::system((oss << "llc -filetype=obj " << n << ".ll -o " << n << ".o", oss.str().c_str()));
      std::cerr << "============ " << r << "\n";
      if (0 != r) throw BaseError("see above");
    }

    // $ clang hello-world.o -o hello-world
    if (link) {
      std::ostringstream oss;
      int r = std::system((oss << "clang " << n << ".o -o " << n, oss.str().c_str()));
      std::cerr << "============ " << r << "\n";
      if (0 != r) throw BaseError("see above");
    }
  }

#if 0
  // this is hell and it does not even work (segfault at `pass->run(module)`)
  // @see https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl08.html
  void VisCodegen::dothething(char const* outfile) {
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

    pass.run(module);
    dest.flush();
  }
#endif


  void VisCodegen::visitNumLiteral(Type const& type, double n) {
    place(std::to_string(n), [n](inject_clo_type also) {
      auto* after = block(std::to_string(n)+"_inject");
      also(after, after, Generated(n));
      point(after);
    });
  }

  void VisCodegen::visitStrLiteral(Type const& type, std::string const& s) {
    place(s, [this, s](inject_clo_type also) {
      auto isread = let<bool>::alloc();
      *isread = false;

      auto len = s.length();
      char const* name = len < 16 ? s.c_str() : ""; // unnamed if too long
      letGlobal<char const**> buffer(name, module, GlobalValue::PrivateLinkage, s, false); // don't add null

      makeStream(name,
        [&isread](BasicBlock* brk, BasicBlock* cont) {
          cond(*isread, brk, cont);
        },
        [&isread, &buffer, len] {
          *isread = true;
          return Generated(*buffer, len);
        },
        also
      );
    });
  }

  void VisCodegen::visitLstLiteral(Type const& type, std::vector<Val*> const& v) {
    throw NIYError("generation of list literals");
  }

  void VisCodegen::visitStrChunks(Type const& type, std::vector<std::string> const& vs) {
    throw NIYError("generation of intermediary 'StrChunks'");
  }

  void VisCodegen::visitFunChain(Type const& type, std::vector<Fun*> const& f) {
    place("chain_of_" + std::to_string(f.size()), [this, &f](inject_clo_type also) {
      size_t k = 0;
      for (auto const& it : f) {
        invoke(*it);
        auto const curr = take();
        place("chain_f_" + std::to_string(++k), [curr](inject_clo_type also) {
          curr.make(also);
        });
      }
      take().make(also);
    });
  }

  void VisCodegen::visitInput(Type const& type) {
    place("input", [this](inject_clo_type also) {
      // TODO: would be nice (and somewhat make sense) just having this let<> as member
      app_t app_f(funname, module);
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
    });
  }

// #define _depth(__depth) _depth_ ## __depth
// #define _depth_0 arg
// #define _depth_1 base->_depth_0
// #define _depth_2 base->_depth_1
// #define _depth_3 base->_depth_2

// #define _bind_some(__count) _bind_some_ ## __count
// #define _bind_some_1(a)          _bind_one(a, 0)
// #define _bind_some_2(a, b)       _bind_one(a, 1); _bind_some_1(b)
// #define _bind_some_3(a, b, c)    _bind_one(a, 2); _bind_some_2(b, c)
// #define _bind_some_4(a, b, c, d) _bind_one(a, 3); _bind_some_3(b, c, d)

// #define _bind_count(__count, ...) _bind_some(__count)(__VA_ARGS__)
// #define _bind_one(__name, __depth) auto& __name = *node._depth(__depth); (void)__name
// #define bind_args(...) _bind_count(__VA_COUNT(__VA_ARGS__), __VA_ARGS__)


  void VisCodegen::visit(bins::abs_::Base const&) {
    place(bins::abs_::name, [this](inject_clo_type also) {
      auto const n = take();

      n.make([&also](BasicBlock* brk, BasicBlock* cont, Generated it) {
        auto* before = here();
        auto* negate = block("abs_negate");
        auto* after = block("abs_after");

        auto in = it.num();
        cond(in < 0., negate, after);

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
    });
  }
  void VisCodegen::visit(bins::abs_ const& node) { place("abs_n", NotHead(*this, *node.arg, *node.base)); }


  void VisCodegen::visit(bins::add_::Base::Base const&) {
    place("add", [this](inject_clo_type also) {
      auto const a = take();
      auto const b = take();
      a.make([&also, &b](BasicBlock *brk, BasicBlock *cont, Generated a) {
        b.make([&also, &a](BasicBlock *brk, BasicBlock *cont, Generated b) {
          also(cont, cont, a.num() + b.num());
        });
        br(cont);
      });
    });
  }
  void VisCodegen::visit(bins::add_::Base const& node) { place("add_a", NotHead(*this, *node.arg, *node.base)); }
  void VisCodegen::visit(bins::add_ const& node) { place("add_a_b", NotHead(*this, *node.arg, *node.base)); }


  void VisCodegen::visit(bins::bytes_::Base const&) {
    place("bytes", [this](inject_clo_type also) {
      auto const s = take();
      s.make([this, &also, &s](BasicBlock* brk, BasicBlock* cont, Generated it) {
        makeBufferLoop("bytes's " + s.name, it.buf().ptr, it.buf().len, [&also](BasicBlock* brk, BasicBlock* cont, let<char> at) {
          also(brk, cont, at.into<double>());
        });
        br(cont);
      });
    });
  }
  void VisCodegen::visit(bins::bytes_ const& node) { place("bytes_s", NotHead(*this, *node.arg, *node.base)); }


  void VisCodegen::visit(bins::const_::Base const&) {
    place("const", [this](inject_clo_type also) {
      take().make(also);
      take();
    });
  }
  void VisCodegen::visit(bins::const_ const& node) { place("const_a", NotHead(*this, *node.arg, *node.base)); }


  void VisCodegen::visit(bins::id_ const&) {
    place("id", [this](inject_clo_type also) {
      take().make(also);
    });
  }


  void VisCodegen::visit(bins::map_::Base::Base const&) {
    place("map", [this](inject_clo_type also) {
      auto const f = take();
      auto const l = take();
      l.make([this, &also, &f](BasicBlock* brk, BasicBlock* cont, Generated it) {
        place("map's it", [=](inject_clo_type also) {
          also(brk, cont, it);
        });
        f.make(also);
        br(cont);
      });
    });
  }
  void VisCodegen::visit(bins::map_::Base const& node) { place("map_f", NotHead(*this, *node.arg, *node.base)); }
  void VisCodegen::visit(bins::map_ const& node) { place("map_f_l", NotHead(*this, *node.arg, *node.base)); }


  void VisCodegen::visit(bins::sub_::Base::Base const&) {
    place(bins::sub_::name, [this](inject_clo_type also) {
      auto const a = take();
      auto const b = take();
      a.make([&also, &b](BasicBlock* brk, BasicBlock* cont, Generated a) {
        b.make([&also, &a](BasicBlock* brk, BasicBlock* cont, Generated b) {
          also(cont, cont, a.num() - b.num());
        });
        br(cont);
      });
    });
  }
  void VisCodegen::visit(bins::sub_::Base const& node) { place("sub_a", NotHead(*this, *node.arg, *node.base)); }
  void VisCodegen::visit(bins::sub_ const& node) { place("sub_a_b", NotHead(*this, *node.arg, *node.base)); }


  // not perfect (eg '1-' is parsed as -1), but will do for now
  void VisCodegen::visit(bins::tonum_::Base const&) {
    place("tonum", [this](inject_clo_type also) {
      auto const s = take();

      auto acc = let<double>::alloc();
      *acc = 0.;

      auto isminus = let<bool>::alloc();
      *isminus = false;
      // auto isdot = let<bool>::alloc();
      // *isdot = false;

      s.make([this, &acc, &isminus](BasicBlock* brk, BasicBlock* cont, Generated it) {
        makeBufferLoop("tonum", it.buf().ptr, it.buf().len, [&acc, &isminus](BasicBlock* brk, BasicBlock* cont, let<char> at) {
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
    });
  }
  void VisCodegen::visit(bins::tonum_ const& node) { place("tonum_s", NotHead(*this, *node.arg, *node.base)); }


  void VisCodegen::visit(bins::tostr_::Base const&) {
    place("tostr", [this](inject_clo_type also) {
      auto const n = take();

      n.make([this, &also](BasicBlock* brk, BasicBlock* cont, Generated it) {
        log << "have @tostr generated\n";

        auto isread = let<bool>::alloc();
        *isread = false;

        auto b_1char = let<char>::alloc(2);
        auto at_2nd_char = b_1char + 1;

        makeStream("tostr",
          [&isread](BasicBlock* brk, BasicBlock* cont) {
            cond(*isread, brk, cont);
          },
          [this, &isread, &b_1char, &at_2nd_char, &it] {
            auto num = it.num();

            log << "call to @tostr, should only be here once\n";
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

            // this is the only iteration, mark end
            *isread = true;

            return Generated(b_1char, total_len);
          },
          also
        );
        br(cont);
      });
    });
  }
  void VisCodegen::visit(bins::tostr_ const& node) { place("tostr_s", NotHead(*this, *node.arg, *node.base)); }


  void VisCodegen::visit(bins::unbytes_::Base const&) {
    place("unbytes", [this](inject_clo_type also) {
      auto const l = take();
      auto b = let<char>::alloc();
      l.make([&also, &b](BasicBlock* brk, BasicBlock* cont, Generated it) {
        *b = it.num().into<char>();
        also(brk, cont, Generated(b, 1));
      });
    });
  }
  void VisCodegen::visit(bins::unbytes_ const& node) { place("unbytes_l", NotHead(*this, *node.arg, *node.base)); }

} // namespace sel

#endif
