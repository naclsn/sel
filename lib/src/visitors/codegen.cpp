#include "sel/visitors.hpp"
#include "sel/parser.hpp"

namespace sel {

#define enter(__n, __w) log << "{{ " << __n << " - " << __w << "\n"; log.indent(); auto _last_entered_n = (__n), _last_entered_w = (__w)
#define leave() log.dedent() << "}} " << _last_entered_n << " - " << _last_entered_w << "\n\n"

#define dumpv(__llval) do {       \
    llvm::raw_os_ostream x(log);  \
    __llval->print(x);            \
} while(0)

  void VisCodegen::num(SyNum sy) {
    switch (pending) {
      case SyType::NONE:
        log << "--- setting new symbol (a number)\n";
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
        log << "--- setting new symbol (a generator)\n";
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
        log << "--- getting existing symbol (a number)\n";
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
        log << "--- getting existing symbol (a generator)\n";
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
    , log(*new indented("   ", std::cerr))
  {
    log << "=== entry\n";

    Val& f = app.get(); // ZZZ: temporary dum accessor
    Type const& ty = f.type();

    if (Ty::FUN != ty.base) {
      std::ostringstream oss;
      throw TypeError((oss << "value of type " << ty << " is not a function", oss.str()));
    }

    // define i32 main()
    llvm::Function::Create(
      llvm::FunctionType::get(llvm::Type::getInt32Ty(context), {}, false),
      llvm::Function::ExternalLinkage,
      "main",
      module
    );

    // input symbol
    visitInput(Type());

    // coerse<Val>(*this, new Input(*this, in), ty.from()); // TODO: input coersion (Str* -> a)
    this->operator()(f);
    // coerse<Str>(*this, (*(Fun*)f)(), Type(Ty::STR, {0}, TyFlag::IS_INF)); // TODO: output coersion (b -> Str*)
  }

  void* VisCodegen::makeOutput() {
    log << "=== exit\n";
    // generate @putbuff

    // last symbol is expected to be of Str
    auto g = gen();
    log << "\n";

    // does not put a new symbol, but actually performs the generation

    // declare i32 @putchar()
    llvm::Function* putchar = llvm::Function::Create(
      llvm::FunctionType::get(llvm::Type::getInt32Ty(context), {llvm::Type::getInt32Ty(context)}, false),
      llvm::Function::ExternalLinkage,
      "putchar",
      module
    );

    llvm::Function* main = module.getFunction("main");
    llvm::BasicBlock* main_entry = llvm::BasicBlock::Create(context, "main_entry", main);
    llvm::BasicBlock* main_exit = llvm::BasicBlock::Create(context, "main_exit", main);

    g.gen(
      builder,
      *main_entry,
      *main_exit,
      [this, putchar] (/*b*/) {
        enter("output", "inject");
        log << "the code for iter for output;\nis a call to @putbuff\n";
        /*b*/
        builder.CreateCall(putchar, {llvm::ConstantInt::get(context, llvm::APInt(32, 42))});
        builder.CreateCall(putchar, {llvm::ConstantInt::get(context, llvm::APInt(32, 10))});
        leave();
      }
    );
    builder.CreateRet(llvm::ConstantInt::get(context, llvm::APInt(32, 0)));

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
    enter("visitFunChain", "");
    log << f.size() << " element/s\n";
    for (auto const& it : f)
      this->operator()(*it);
    leave();
  }

  void VisCodegen::visitInput(Type const& type) {
    enter("visitInput", "");
    gen(SyGen("input",
      [this] {
        enter("input", "entry");
        // declare i32 @getchar()
        llvm::Function* getchar = llvm::Function::Create(
          llvm::FunctionType::get(llvm::Type::getInt32Ty(context), {}, false),
          llvm::Function::ExternalLinkage,
          "getchar",
          module
        );
        leave();
        return getchar; // ZZZ
      },

      [this] (llvm::BasicBlock* brk, llvm::BasicBlock* cont, llvm::Value*) {
        enter("input", "check");
        log << "check eof\n";
        builder.CreateCondBr(llvm::ConstantInt::get(context, llvm::APInt(1, 1)), brk, cont);
        leave();
      },

      [this] (llvm::Value* getchar) {
        enter("input", "iter");
        log << "get some input\n";
        builder.CreateCall(getchar, {}, "char"); // ZZZ
        leave();
      }
    ));
    leave();
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
    enter(bins::tonum_::name, "");
    auto g = gen();
    num(SyNum(
      bins::tonum_::name,
      [this, g] {
        enter(bins::tonum_::name, "value");
        auto* stored = builder.GetInsertBlock();

        llvm::Function* tonum_outline = llvm::Function::Create(
          llvm::FunctionType::get(llvm::Type::getInt32Ty(context), {}, false),
          llvm::Function::ExternalLinkage,
          "tonum_outline",
          module
        );
        llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "tonum_arg_entry", tonum_outline);
        llvm::BasicBlock* exit = llvm::BasicBlock::Create(context, "tonum_arg_exit", tonum_outline);
        builder.SetInsertPoint(entry);

        /*n=*/g.gen(
          builder,
          *entry,
          *exit,
          [this] (/*b*/) {
            enter(bins::tonum_::name, "inject");
            log << "the code for iter;\ncheck if still numeric;\n*10+ accumulate;\n";
            leave();
          }
        );
        builder.CreateRet(/*n*/
          llvm::ConstantInt::get(context, llvm::APInt(32, 110)));

        builder.SetInsertPoint(stored);
        /*n=*/builder.CreateCall(tonum_outline);
        leave();
        /*return n*/
      }
    ));
    leave();
  }

  void VisCodegen::visit(bins::tonum_ const& node) {
    throw NIYError("codegen tonum no arg");
  }


  void VisCodegen::visit(bins::tostr_::Base const& node) {
    enter(bins::tostr_::name, "");
    auto g = num();
    gen(SyGen(
      bins::tostr_::name,
      [this] {
        enter(bins::tostr_::name, "entry");
        log << "have @tostr generated\n";
        log << "%_isend = alloca i1\n";
        llvm::Value* isend = builder.CreateAlloca(llvm::Type::getInt1Ty(context), nullptr, "tostr_isend");
        builder.CreateStore(isend, llvm::ConstantInt::get(context, llvm::APInt(1, 0)));
        leave();
        return isend;
      },

      [this] (llvm::BasicBlock* brk, llvm::BasicBlock* cont, llvm::Value* isend) {
        enter(bins::tostr_::name, "check");
        log << "branch if %tostr_isend (ie. 1 iteration)\n";
        builder.CreateCondBr(isend, brk, cont);
        leave();
      },

      [this, g] (llvm::Value* isend) {
        enter(bins::tostr_::name, "iter");
        /*n=*/ g.gen();
        log << "call to @tostr, should only be here once\n";
        /*b=*/
        log << "store i1 1 %_isend\n";
        builder.CreateStore(isend, llvm::ConstantInt::get(context, llvm::APInt(1, 1)));
        leave();
        /*return b*/
      }
    ));
    leave();
  }

  void VisCodegen::visit(bins::tostr_ const& node) {
    throw NIYError("codegen tostr no arg");
  }

} // namespace sel
