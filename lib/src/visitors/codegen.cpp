#include "sel/visitors.hpp"
#include "sel/parser.hpp"

namespace sel {

  void VisCodegen::num(SyNum sy) {
    switch (pending) {
      case SyType::NONE:
        std::cerr << "--- setting new symbol (a number)\n";
        synum = std::move(sy);
        pending = SyType::SYNUM;
        break;
      case SyType::SYNUM: throw BaseError("codegen failure: overriding an unused symbol (a number)");
      case SyType::SYGEN: throw BaseError("codegen failure: overriding an unused symbol (a generator)");
    }
  }

  void VisCodegen::gen(SyGen sy) {
    switch (pending) {
      case SyType::NONE:
        std::cerr << "--- setting new symbol (a generator)\n";
        sygen = std::move(sy);
        pending = SyType::SYGEN;
        break;
      case SyType::SYNUM: throw BaseError("codegen failure: overriding an unused symbol (a number)");
      case SyType::SYGEN: throw BaseError("codegen failure: overriding an unused symbol (a generator)");
    }
  }

  VisCodegen::SyNum VisCodegen::num() {
    switch (pending) {
      case SyType::SYNUM:
        std::cerr << "--- getting existing symbol (a number)\n";
        pending = SyType::NONE;
        return std::move(synum);
      case SyType::NONE: throw BaseError("codegen failure: no pending symbol to access");
      case SyType::SYGEN: throw BaseError("codegen failure: symbol type does not match (expected a number)");
    }
    throw "unreachable"; // shuts warnings
  }

  VisCodegen::SyGen VisCodegen::gen() {
    switch (pending) {
      case SyType::SYGEN:
        std::cerr << "--- getting existing symbol (a generator)\n";
        pending = SyType::NONE;
        return std::move(sygen);
      case SyType::NONE: throw BaseError("codegen failure: no pending symbol to access");
      case SyType::SYNUM: throw BaseError("codegen failure: symbol type does not match (expected a generator)");
    }
    throw "unreachable"; // shuts warnings
  }

  VisCodegen::VisCodegen(char const* module_name, App& app)
    : context()
    , builder(context)
    , module(module_name, context)
    , log(std::cerr)
  {
    log << "=== entry\n";

    Val& f = app.get();
    Type const& ty = f.type();

    if (Ty::FUN != ty.base) {
      std::ostringstream oss;
      throw TypeError((oss << "value of type " << ty << " is not a function", oss.str()));
    }

    // input symbol (could be in visitInput itself i guess)
    gen(SyGen(
      [this] {
        log << "input: things to do at startup\n";
      },
      [this] {
        log << "input: check eof\n";
      },
      [this] {
        log << "input: get some input\n";
      }
    ));

    // coerse<Val>(*this, new Input(*this, in), ty.from()); // input coersion (Str* -> a)
    this->operator()(f);
    // coerse<Str>(*this, (*(Fun*)f)(), Type(Ty::STR, {0}, TyFlag::IS_INF)); // output coersion (b -> Str*)
  }

  void* VisCodegen::makeOutput() {
    log << "=== exit\n";
    // generate @putbuff

    // last symbol is expected to be of Str
    auto g = gen();
    log << "\n";
    // does not put a new symbol, but actually performs the generation
    g.genEntry();

    g.genCheck();
    g.genIter([this] {
      log << "output: the code for iter for output; is a call to @putbuff\n";
    });

    return nullptr; // `void*` just a placeholder type
  }


  void VisCodegen::visitNumLiteral(Type const& type, double n) {
  }

  void VisCodegen::visitStrLiteral(Type const& type, std::string const& s) {
  }

  void VisCodegen::visitLstLiteral(Type const& type, std::vector<Val*> const& v) {
  }

  void VisCodegen::visitStrChunks(Type const& type, std::vector<std::string> const& vs) {
  }

  void VisCodegen::visitFunChain(Type const& type, std::vector<Fun*> const& f) {
    log << "visitFunChain: " << f.size() << " element/s\n";
    for (auto const& it : f) {
      log << '\t' << repr(*it) << '\n';
      this->operator()(*it);
    }
  }

  void VisCodegen::visitInput(Type const& type) {
  }

#define _depth(__depth) _depth_ ## __depth
#define _depth_0 arg
#define _depth_1 base->_depth_0
#define _depth_2 base->_depth_1
#define _depth_3 base->_depth_2

#define _bind_some(__count) _bind_some_ ## __count
#define _bind_some_1(a)          _bind_one(a, 0)
#define _bind_some_2(a, b)       _bind_one(a, 1); _bind_some_1(b)
#define _bind_some_3(a, b, c)    _bind_one(a, 2); _bind_some_2(b, c)
#define _bind_some_4(a, b, c, d) _bind_one(a, 3); _bind_some_3(b, c, d)

#define _bind_count(__count, ...) _bind_some(__count)(__VA_ARGS__)
#define _bind_one(__name, __depth) auto& __name = *node._depth(__depth); (void)__name
#define bind_args(...) _bind_count(__VA_COUNT(__VA_ARGS__), __VA_ARGS__)

  void VisCodegen::visit(bins::abs_::Base const& node) {
  }
  void VisCodegen::visit(bins::abs_ const& node) {
    this->operator()(*node.arg);
  }

  // void VisCodegen::visit(bins::add_::Base::Base const& node) {
  // }
  // void VisCodegen::visit(bins::add_::Base const& node) {
  // }
  // void VisCodegen::visit(bins::add_ const& node) {
  //   bind_args(a, b);
  //   this->operator()(a);
  //   this->operator()(b);
  // }

  void VisCodegen::visit(bins::tonum_::Base const& node) {
    log << "tonum_\n";
    auto g = gen();
    num(SyNum([this, g] {
      log << "tonum_: gen for value asked\n";

      g.genEntry();

      g.genCheck();
      g.genIter([this] {
        log << "tonum_: the code for iter; check if still numeric; *10+ accumulate;\n";
      });
    }));
  }
  void VisCodegen::visit(bins::tonum_ const& node) {
  }

  void VisCodegen::visit(bins::tostr_::Base const& node) {
    log << "tostr_\n";
    auto g = num();
    gen(SyGen(
      [this] {
        log << "tostr_: have @tostr generated, alloca i1 'read'\n";
      },
      [this] {
        log << "tostr_: isend = false once then true\n";
      },
      [this, g] {
        /*n=*/ g.genValue();
        log << "tostr_: call to @tostr, should only be here once\n";
      }
    ));
  }
  void VisCodegen::visit(bins::tostr_ const& node) {
  }

} // namespace sel
