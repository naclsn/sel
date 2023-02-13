#include "sel/visitors.hpp"
#include "sel/parser.hpp"

namespace sel {

  VisCodegen::Symbol::Symbol(Symbol const& other) {
    switch (sty = other.sty) {
      case SyType::SYNUM: v.synum = other.v.synum; break;
      case SyType::SYGEN: v.sygen = other.v.sygen; break;
    }
  }

  VisCodegen::Symbol::~Symbol() {
    switch (sty) {
      case SyType::SYNUM: v.synum.~SymNumber(); break;
      case SyType::SYGEN: v.sygen.~SymGenerator(); break;
    }
  }

  VisCodegen::Symbol& VisCodegen::Symbol::operator=(Symbol const& other) {
    switch (sty = other.sty) {
      case SyType::SYNUM: v.synum = other.v.synum; break;
      case SyType::SYGEN: v.sygen = other.v.sygen; break;
    }
    return *this;
  }

  void VisCodegen::setSym(Symbol const& sym) {
    if (symbol_pending && !symbol_used)
      throw BaseError("codegen failure: overriding an unused symbol");
    symbol = sym;
    symbol_pending = true;
    symbol_used = false;
  }

  VisCodegen::SymNumber const VisCodegen::getSymNumber() {
    if (!symbol_pending)
      throw BaseError("codegen failure: no pending symbol to access");
    if (Symbol::SyType::SYNUM != symbol.sty) {
      throw BaseError("codegen failure: symbol type does not match (expected a number)");
    }
    symbol_used = true;
    symbol_pending = false;
    return symbol.v.synum;
  }

  VisCodegen::SymGenerator const VisCodegen::getSymGenerator() {
    if (!symbol_pending)
      throw BaseError("codegen failure: no pending symbol to access");
    if (Symbol::SyType::SYGEN != symbol.sty) {
      throw BaseError("codegen failure: symbol type does not match (expected a generator)");
    }
    symbol_used = true;
    symbol_pending = false;
    return symbol.v.sygen;
  }


  VisCodegen::VisCodegen(char const* module_name, App& app)
    : context()
    , builder(context)
    , module(module_name, context)
  {
    std::cerr << "=== entry\n";

    Val& f = app.get();
    Type const& ty = f.type();

    if (Ty::FUN != ty.base) {
      std::ostringstream oss;
      throw TypeError((oss << "value of type " << ty << " is not a function", oss.str()));
    }

    // input symbol
    setSym(SymGenerator(
      [] {
        std::cerr << "input: things to do at startup\n";
      },
      [] {
        std::cerr << "input: check eof\n";
      },
      [] {
        std::cerr << "input: get some input\n";
      }
    ));

    // coerse<Val>(*this, new Input(*this, in), ty.from()); // input coersion (Str* -> a)
    this->operator()(f);
    // coerse<Str>(*this, (*(Fun*)f)(), Type(Ty::STR, {0}, TyFlag::IS_INF)); // output coersion (b -> Str*)
  }

  void* VisCodegen::makeOutput() {
    std::cerr << "=== exit\n";
    // generate @putbuff

    // last symbol is expected to be of Str
    auto g = getSymGenerator();
    // does not put a new symbol, but actually performs the generation
    g.genEntry();

    g.genIter([] {
      std::cerr << "output: the code for iter for output; is a call to @putbuff\n";
    });
    g.genCheck();

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
    std::cerr << "visitFunChain: " << f.size() << " element/s\n";
    for (auto const& it : f) {
      std::cerr << '\t' << repr(*it) << '\n';
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
    std::cerr << "tonum_\n";
    auto g = getSymGenerator();
    setSym(SymNumber([g] {
      std::cerr << "tonum_: gen for value asked\n";

      g.genEntry();

      g.genIter([] {
        std::cerr << "tonum_: the code for iter; check if still numeric; *10+ accumulate;\n";
      });
      g.genCheck();
    }));
  }
  void VisCodegen::visit(bins::tonum_ const& node) {
  }

  void VisCodegen::visit(bins::tostr_::Base const& node) {
    std::cerr << "tostr_\n";
    auto g = getSymNumber();
    setSym(SymGenerator(
      [] {
        std::cerr << "tostr_: have @tostr generated, alloca i1 'read'\n";
      },
      [] {
        std::cerr << "tostr_: isend = false once then true\n";
      },
      [g] {
        /*n=*/ g.genValue();
        std::cerr << "tostr_: call to @tostr, should only be here once\n";
      }
    ));
  }
  void VisCodegen::visit(bins::tostr_ const& node) {
  }

} // namespace sel
