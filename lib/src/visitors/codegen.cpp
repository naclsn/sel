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

    inline let<double> const& num() const { return _num; }
    inline decltype(_buf) const& buf() const { return _buf; }

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
    cond(len == let<int>(0), after_bb, body_bb);

    point(body_bb);
    // when first entering, init to 0
    letPHI<int> k;
    k.incoming(let<int>(0), before_bb);

    // actual loop body
    // body -> incr   after
    //  `-------------^
    body(after_bb, incr_bb, (Value*)let<char>(ptr[k]));

    point(incr_bb);
    // when from continue, take the ++
    auto kpp = k + let<int>(1);
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
    systack.push(Symbol(name, doobidoo));
  }

  VisCodegen::Symbol const VisCodegen::take() {
    Symbol r = systack.top();
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


  VisCodegen::VisCodegen(char const* file_name, char const* module_name, App& app)
    : context()
    , builder(context)
    , module(module_name, context)
    , log(*new indented("   ", std::cerr))
  {
    tll::builder(builder);

    module.setSourceFileName(file_name);
    log << "=== entry\n";

    Val& f = app.get(); // ZZZ: temporary dum accessor
    Type const& ty = f.type();

    if (Ty::FUN != ty.base) {
      std::ostringstream oss;
      throw TypeError((oss << "value of type " << ty << " is not a function", oss.str()));
    }

    let<int()> main("main", module, Function::ExternalLinkage);

    // input symbol
    visitInput(Type());

    // coerse<Val>(app, new Input(app, in), ty.from()); // TODO: input coersion (Str* -> a)
    invoke(f);
    // coerse<Str>(app, (*(Fun*)f)(), Type(Ty::STR, {0}, TyFlag::IS_INF)); // TODO: output coersion (b -> Str*)
  }

  void* VisCodegen::makeOutput() {
    log << "=== exit\n\n";

    let<int(int)> putchar("putchar", module, Function::ExternalLinkage);

    // get existing "main"
    let<int()> main("main", module);
    point(main["entry"]);

    // does not put a new symbol, but actually performs the generation

    take().make([=] (BasicBlock* brk, BasicBlock* cont, Generated it) {
      makeBufferLoop("output", it.buf().ptr, it.buf().len, [&] (BasicBlock* brk, BasicBlock* cont, let<char> at) {
        putchar(at.into<int>());
        br(cont);
      });
      br(cont);
    });

    main.ret(0);

    if (!systack.empty()) throw CodegenError("symbol stack should be empty at the end");

    //main->viewCFG();
    return nullptr; // `void*` just a placeholder type
  }


  // temporary dothething
  void VisCodegen::dothething(char const* outfile) {
    std::string const n = outfile;

    {
      std::ofstream ll(n + ".ll");
      if (!ll.is_open()) throw BaseError("could not open file");
      dump(ll);
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
    {
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
    place(std::to_string(n), [n] (inject_clo_type also) {
      auto* after = block(std::to_string(n)+"_inject");
      also(after, after, let<double>(n));
      point(after);
    });
  }

  void VisCodegen::visitStrLiteral(Type const& type, std::string const& s) {
    place(s, [this, s] (inject_clo_type also) {
      auto isread = let<bool>::alloc();
      *isread = false;

      char const* name = s.length() < 16 ? s.c_str() : ""; // unnamed if too long
      auto buffer = let<char const**>(name, module, GlobalValue::PrivateLinkage, s, false); // don't add null

      makeStream(name,
        [=] (BasicBlock* brk, BasicBlock* cont) {
          cond(*isread, brk, cont);
        },
        [=] {
          *isread = true;
          return Generated(*buffer, let<int>(s.length()));
        },
        also
      );
    });
  }

  void VisCodegen::visitLstLiteral(Type const& type, std::vector<Val*> const& v) {
  }

  void VisCodegen::visitStrChunks(Type const& type, std::vector<std::string> const& vs) {
  }

  void VisCodegen::visitFunChain(Type const& type, std::vector<Fun*> const& f) {
    place("chain_of_" + std::to_string(f.size()), [=] (inject_clo_type also) {
      size_t k = 0;
      for (auto const& it : f) {
        invoke(*it);
        auto const curr = take();
        place("chain_f_" + std::to_string(++k), [=] (inject_clo_type also) {
          curr.make(also);
        });
      }
      take().make(also);
    });
  }

  void VisCodegen::visitInput(Type const& type) {
    place("input", [=] (inject_clo_type also) {
      let<int()> getchar("getchar", module, Function::ExternalLinkage);

      auto b_1char = let<char>::alloc();

      auto chk = [=] (BasicBlock* brk, BasicBlock* cont) {
        auto read_char = getchar().into<char>();
        *b_1char = read_char;
        // check for eof (-1)
        cond(read_char == -1, brk, cont);
      };

      auto itr = [=] {
        // simply returns the character read in the last pass through block 'check'
        // XXX: for now this is a 1-char buffer
        return Generated(b_1char, let<int>(1));
      };

      makeStream("input", chk, itr, also);
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


  void VisCodegen::visit(bins::abs_::Base const& node) {
    place(bins::abs_::name, [=] (inject_clo_type also) {
      auto const n = take();

      n.make([=] (BasicBlock* brk, BasicBlock* cont, Generated it) {
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
            std::make_pair(in, before),
            std::make_pair(pos, negate),
          });

        also(cont, cont, out);
      });
    });
  }

  void VisCodegen::visit(bins::abs_ const& node) {
    place("abs_n", [=] (inject_clo_type also) {
      invoke(*node.arg);
      invoke(*node.base);
      take().make(also);
    });
  }


  void VisCodegen::visit(bins::add_::Base::Base const& node) {
    place("add", [=] (inject_clo_type also) {
      auto const a = take();
      auto const b = take();
      a.make([=] (BasicBlock *brk, BasicBlock *cont, Generated a) {
        b.make([=] (BasicBlock *brk, BasicBlock *cont, Generated b) {
          also(cont, cont, a.num() + b.num());
        });
        br(cont);
      });
    });
  }

  void VisCodegen::visit(bins::add_::Base const& node) {
    place("add_a", [=] (inject_clo_type also) {
      invoke(*node.arg);
      invoke(*node.base);
      take().make(also);
    });
  }

  void VisCodegen::visit(bins::add_ const& node) {
    place("add_a_b", [=] (inject_clo_type also) {
      invoke(*node.arg);
      invoke(*node.base);
      take().make(also);
    });
  }


  void VisCodegen::visit(bins::bytes_::Base const& node) {
    place("bytes", [=] (inject_clo_type also) {
      auto const s = take();
      s.make([=] (BasicBlock* brk, BasicBlock* cont, Generated it) {
        makeBufferLoop("bytes's " + s.name, it.buf().ptr, it.buf().len, [=] (BasicBlock* brk, BasicBlock* cont, let<char> at) {
          also(brk, cont, at.into<double>());
        });
        br(cont);
      });
    });
  }

  void VisCodegen::visit(bins::bytes_ const& node) {
    place("bytes_s", [=] (inject_clo_type also) {
      invoke(*node.arg);
      invoke(*node.base);
      take().make(also);
    });
  }


  void VisCodegen::visit(bins::id_ const& node) {
    place("id", [=] (inject_clo_type also) {
      take().make(also);
    });
  }


  void VisCodegen::visit(bins::map_::Base::Base const& node) {
    place("map", [=] (inject_clo_type also) {
      auto const f = take();
      auto const l = take();
      l.make([=] (BasicBlock* brk, BasicBlock* cont, Generated it) {
        place("map's it", [=] (inject_clo_type also) {
          also(brk, cont, it);
        });
        f.make(also);
        br(cont);
      });
    });
  }

  void VisCodegen::visit(bins::map_::Base const& node) {
    place("map_f", [=] (inject_clo_type also) {
      invoke(*node.arg);
      invoke(*node.base);
      take().make(also);
    });
  }

  void VisCodegen::visit(bins::map_ const& node) {
    place("map_f_l", [=] (inject_clo_type also) {
      invoke(*node.arg);
      invoke(*node.base);
      take().make(also);
    });
  }


  void VisCodegen::visit(bins::sub_::Base::Base const& node) {
    place(bins::sub_::name, [=] (inject_clo_type also) {
      auto const a = take();
      auto const b = take();
      a.make([=] (BasicBlock* brk, BasicBlock* cont, Generated a) {
        b.make([=] (BasicBlock* brk, BasicBlock* cont, Generated b) {
          also(cont, cont, a.num() - b.num());
        });
        br(cont);
      });
    });
  }

  void VisCodegen::visit(bins::sub_::Base const& node) {
    place("sub_a", [=] (inject_clo_type also) {
      invoke(*node.arg);
      invoke(*node.base);
      take().make(also);
    });
  }

  void VisCodegen::visit(bins::sub_ const& node) {
    place("sub_a_b", [=] (inject_clo_type also) {
      invoke(*node.arg);
      invoke(*node.base);
      take().make(also);
    });
  }


  // not perfect (eg '1-' is parsed as -1), but will do for now
  void VisCodegen::visit(bins::tonum_::Base const& node) {
    place("tonum", [=] (inject_clo_type also) {
      auto const s = take();

      auto acc = let<double>::alloc();
      *acc = 0.;

      auto isminus = let<bool>::alloc();
      *isminus = false;
      // auto isdot = let<bool>::alloc();
      // *isdot = false;

      s.make([=] (BasicBlock* brk, BasicBlock* cont, Generated it) {
        makeBufferLoop("tonum", it.buf().ptr, it.buf().len, [&] (BasicBlock* brk, BasicBlock* cont, let<char> at) {
          // _ -> setnegate -> after
          // `-> accumulate ---^
          auto* setnegate = block("tonum_setnegate");
          auto* accumulate = block("tonum_accumulate");
          auto* after = block("tonum_after");

          auto isnegate = at == '-' && !let<bool>(*isminus);
          cond(isnegate, setnegate, accumulate);

          point(setnegate);
            *isminus = true;
            br(after);

          point(accumulate);
            auto* acc_if_isdigit = block("tonum_acc_if_isdigit");
            // break if no longer digit
            cond(at >= '0' && at <= '9', acc_if_isdigit, brk);

            point(acc_if_isdigit);
              *acc = let<double>(*acc)*10 + (at-let<char>('0')).into<double>();

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
          std::make_pair(nres, negate),
          std::make_pair(res, nonegate),
        });

      auto* after = block("tonum_inject");
      also(after, after, final_res);
      point(after);
    });
  }

  void VisCodegen::visit(bins::tonum_ const& node) {
    place("tonum_s", [=] (inject_clo_type also) {
      invoke(*node.arg);
      invoke(*node.base);
      take().make(also);
    });
  }


  void VisCodegen::visit(bins::tostr_::Base const& node) {
    place("tostr", [=] (inject_clo_type also) {
      auto const n = take();

      n.make([=] (BasicBlock* brk, BasicBlock* cont, Generated it) {
        log << "have @tostr generated\n";

        auto isread = let<bool>::alloc();
        *isread = false;

        auto b_1char = let<char>::alloc(2);
        auto at_2nd_char = b_1char + 1;

        auto chk = [=] (BasicBlock* brk, BasicBlock* cont) {
          cond(*isread, brk, cont);
        };

        auto itr = [=] {
          auto num = let<double>(it.num());

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
              std::make_pair(pnum, minus),
              std::make_pair(num, nominus),
            });

            letPHI<char*> store_at({
              std::make_pair(at_2nd_char, minus),
              std::make_pair(b_1char, nominus),
            });

            letPHI<int> total_len({
              std::make_pair(2, minus),
              std::make_pair(1, nominus),
            });

            // also for now 1-char buffer, we only do digits here
            *store_at = positive.into<char>() + let<char>('0');

          // this is the only iteration, mark end
          *isread = true;

          return Generated(b_1char, total_len);
        };

        makeStream("tostr", chk, itr, also);
        br(cont);
      });
    });
  }

  void VisCodegen::visit(bins::tostr_ const& node) {
    place("tostr_s", [=] (inject_clo_type also) {
      invoke(*node.arg);
      invoke(*node.base);
      take().make(also);
    });
  }


  void VisCodegen::visit(bins::unbytes_::Base const& node) {
    place("unbytes", [=] (inject_clo_type also) {
      auto const l = take();
      auto b = let<char>::alloc();
      l.make([=] (BasicBlock* brk, BasicBlock* cont, Generated it) {
        *b = let<double>(it.num()).into<char>();
        also(brk, cont, Generated(b, let<int>(1)));
      });
    });
  }

  void VisCodegen::visit(bins::unbytes_ const& node) {
    place("unbytes_l", [=] (inject_clo_type also) {
      invoke(*node.arg);
      invoke(*node.base);
      take().make(also);
    });
  }

} // namespace sel
