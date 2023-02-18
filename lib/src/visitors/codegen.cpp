#include <fstream>

#include "sel/visitors.hpp"
#include "sel/parser.hpp"

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>

using namespace llvm;

namespace sel {

  void VisCodegen::makeBufferLoop(std::string const name, Value* ptr, Value* len, clo_loop body) {
    // {before} -> loop -> after
    //      \      `--'     ^
    //       `-------------'
    auto* before_bb = builder.GetInsertBlock();
    auto* loop_bb = makeBlock(builder, name+"_loop_body");
    auto* after_bb = makeBlock(builder, name+"_loop_break");

    // before -> {loop}    after
    //     `----------------^
    auto* isempty = builder.CreateICmpEQ(len, len_val(0), name+"_loop_isempty");
    builder.CreateCondBr(isempty, after_bb, loop_bb);
    builder.SetInsertPoint(loop_bb);

    // when first entering, init to 0
    auto* k = builder.CreatePHI(len_type, 2, name+"_loop_k");
    k->addIncoming(len_val(0), before_bb);

    // actual loop body
    auto* it = builder.CreateGEP(chr_type, ptr, k, name+"_loop_it");
    auto* at = builder.CreateLoad(it, name+"_loop_at");
    body(at);

    // when from continue, take the ++
    auto* kpp = builder.CreateAdd(k, len_val(1), name+"_loop_kpp");
    k->addIncoming(kpp, builder.GetInsertBlock()); // block 'body' ends in (which can be loop_bb if it did not create any)

    // loop -> {after}
    // `--'
    auto* isend = builder.CreateICmpEQ(len, kpp, name+"_loop_isend");
    builder.CreateCondBr(isend, after_bb, loop_bb);
    builder.SetInsertPoint(after_bb);
  }

  void VisCodegen::makeBufferLoop(std::string const name, std::pair<Value*, Value*> ptr_len, clo_loop body) {
    makeBufferLoop(name, std::get<0>(ptr_len), std::get<1>(ptr_len), body);
  }

  template <typename Sy> struct sytype_for_sy;
  template <> struct sytype_for_sy<VisCodegen::SyNum> { static constexpr VisCodegen::SyType the = VisCodegen::SyType::SYNUM; };
  template <> struct sytype_for_sy<VisCodegen::SyGen> { static constexpr VisCodegen::SyType the = VisCodegen::SyType::SYGEN; };

  template <typename Sy> struct sy_to_string;
  template <> struct sy_to_string<VisCodegen::SyNum> { static constexpr char const* the = "number"; };
  template <> struct sy_to_string<VisCodegen::SyGen> { static constexpr char const* the = "generator"; };

  template <typename SyTake, typename SyPlace>
  void VisCodegen::replaceSymbol(std::function<SyPlace(SyTake const&)> replacer) {
    if (sytype_for_sy<SyTake>::the != pending) {
      std::ostringstream oss;
      throw CodegenError((oss
        << "expected symbol to be a " << sy_to_string<SyTake>::the
        << " (replacing with a " << sy_to_string<SyPlace>::the << ")"
      , oss.str()));
    }
    pending = sytype_for_sy<SyPlace>::the;
    log << "< taking " << quoted(_replaceTake<SyTake>().name) << "\n";
    log << "> placing " << quoted(_replaceTake<SyPlace>().name) << "\n";
    _replacePlace<SyPlace>() = replacer(_replaceTake<SyTake>());
  }
  template <> VisCodegen::SyNum& VisCodegen::_replacePlace<VisCodegen::SyNum>() { return synum; }
  template <> VisCodegen::SyGen& VisCodegen::_replacePlace<VisCodegen::SyGen>() { return sygen; }
  template <> VisCodegen::SyNum const& VisCodegen::_replaceTake<VisCodegen::SyNum>() { return synum; }
  template <> VisCodegen::SyGen const& VisCodegen::_replaceTake<VisCodegen::SyGen>() { return sygen; }

  Value* VisCodegen::SyNum::make() const {
    // (val may create new blocks, as long as it is internally consitent)
    return val();
  }

  void VisCodegen::SyGen::make(IRBuilder<>& builder, inject_clo_type also) const {
    // entry -> check -> [iter -> inject] (inject must branch to exit or check)
    //           |  ^-------------' v
    //           `-------------> exit (insert point ends up in exit, which is empty)

    // {entry}
    auto chk_itr = ent();
    auto chk = std::get<0>(chk_itr);
    auto itr = std::get<1>(chk_itr);

    // entry -> {check}
    auto* check = makeBlock(builder, name+"_check");
    builder.CreateBr(check);
    builder.SetInsertPoint(check);

    auto* exit = makeBlock(builder, name+"_exit");
    auto* iter = makeBlock(builder, name+"_iter");
    // (responsibility of chk)
    // {check}  -> [iter
    //    `--------> exit
    chk(exit, iter);

    // [{iter}
    builder.SetInsertPoint(iter);
    auto ptr_len = itr();

    // [iter -> {inject}]
    auto* inject = makeBlock(builder, name+"_inject");
    builder.CreateBr(inject);
    builder.SetInsertPoint(inject);
    // (responsibility of also)
    // check -> [iter -> inject]
    //  |  ^-------------' v
    //  `-------------> exit
    also(exit, check, ptr_len);

    // {exit}
    builder.SetInsertPoint(exit);
  }


#define enter(__n, __w) log << "{{ " << __n << " - " << __w << "\n"; log.indent(); auto _last_entered_n = (__n), _last_entered_w = (__w)
#define leave() log.dedent() << "}} " << _last_entered_n << " - " << _last_entered_w << "\n\n"

  VisCodegen::VisCodegen(char const* file_name, char const* module_name, App& app)
    : context()
    , builder(context)
    , module(module_name, context)
    , log(*new indented("   ", std::cerr))

    , bool_type(llvm::Type::getInt1Ty(context))
    , bool_true(ConstantInt::get(context, APInt(1, 1)))
    , bool_false(ConstantInt::get(context, APInt(1, 0)))
    , num_type(llvm::Type::getDoubleTy(context))
    , chr_type(llvm::Type::getInt8Ty(context))
    , len_type(llvm::Type::getInt32Ty/*64*/(context))
  {
    module.setSourceFileName(file_name);
    log << "=== entry\n";

    Val& f = app.get(); // ZZZ: temporary dum accessor
    Type const& ty = f.type();

    if (Ty::FUN != ty.base) {
      std::ostringstream oss;
      throw TypeError((oss << "value of type " << ty << " is not a function", oss.str()));
    }

    // define i32 main()
    Function::Create(
      FunctionType::get(llvm::Type::getInt32Ty(context), {}, false),
      Function::ExternalLinkage,
      "main",
      module
    );

    // input symbol
    visitInput(Type());

    // coerse<Val>(app, new Input(app, in), ty.from()); // TODO: input coersion (Str* -> a)
    this->operator()(f);
    // coerse<Str>(app, (*(Fun*)f)(), Type(Ty::STR, {0}, TyFlag::IS_INF)); // TODO: output coersion (b -> Str*)
  }

  void* VisCodegen::makeOutput() {
    log << "=== exit\n";

    // final symbol is expected to be of a Str
    if (SyType::SYGEN != pending)
      throw CodegenError("expected final symbol to be a generator");
    log << "\n";

    // does not put a new symbol, but actually performs the generation

    // declare i32 @putchar()
    Function* putchar = Function::Create(
      FunctionType::get(llvm::Type::getInt32Ty(context), {llvm::Type::getInt32Ty(context)}, false),
      Function::ExternalLinkage,
      "putchar",
      module
    );

    auto* main = module.getFunction("main");
    builder.SetInsertPoint(BasicBlock::Create(context, "entry", main));
    sygen.make(
      builder,
      [=] (BasicBlock* brk, BasicBlock* cont, std::pair<Value*, Value*> ptr_len) {
        enter("output", "inject");

        makeBufferLoop("output", ptr_len, [&] (Value* at) {
          auto* write_xchar = builder.CreateZExt(at, llvm::Type::getInt32Ty(context), "output_xchar");
          builder.CreateCall(putchar, {write_xchar}, "_");
        });

        // ZZZ: this just so produced output easier to understand
        builder.CreateCall(putchar, {ConstantInt::get(context, APInt(32, '\n'))}, "_");

        // no reason to break out, just continue
        builder.CreateBr(cont);
        leave();
      }
    );
    builder.CreateRet(ConstantInt::get(context, APInt(32, 0)));

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
    synum = SyNum(std::to_string(n), [this, n] {
      log << "this should be a\n";
      return num_val(n);
    });
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
    sygen = SyGen("input",
      [=] {
        enter("input", "entry");

        // declare i32 @getchar()
        Function* getchar = Function::Create(
          FunctionType::get(llvm::Type::getInt32Ty(context), {}, false),
          Function::ExternalLinkage,
          "getchar",
          module
        );

        auto* b_1char = builder.CreateAlloca(chr_type, nullptr, "input_b_1char");

        auto chk = [=] (BasicBlock* brk, BasicBlock* cont) {
          enter("input", "check");

          auto* read_xchar = builder.CreateCall(getchar, {}, "input_xchar");
          auto* read_char = builder.CreateTrunc(read_xchar, chr_type, "input_char");
          builder.CreateStore(read_char, b_1char);

          // check for eof (-1)
          auto* iseof = builder.CreateICmpEQ(chr_val(-1), read_char, "input_iseof");
          builder.CreateCondBr(iseof, brk, cont);

          leave();
        };

        auto itr = [=] () {
          enter("input", "iter");
          // simply returns the character read in the last pass through block 'check'
          // XXX: for now this is a 1-char buffer
          leave();
          return std::make_pair(b_1char, len_val(1));
        };

        leave();
        return std::make_pair(chk, itr);
      }
    );
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
    replaceSymbol<SyNum, SyNum>([this] (SyNum const& n) { return SyNum(bins::abs_::name, [=] {
      auto* in = n.make();

      auto* before = builder.GetInsertBlock();
      auto* negate = makeBlock(builder, "abs_negate");
      auto* after = makeBlock(builder, "abs_after");

      auto* isnegative = builder.CreateFCmpOLT(in, num_val(0), "abs_isnegative");
      builder.CreateCondBr(isnegative, negate, after);

      builder.SetInsertPoint(negate);
        auto* pos = builder.CreateFNeg(in, "abs_pos");
        builder.CreateBr(after);

      builder.SetInsertPoint(after);
        auto* out = builder.CreatePHI(num_type, 2, "abs_out");
        out->addIncoming(in, before);
        out->addIncoming(pos, negate);

      return out;
    }); });
  }

  void VisCodegen::visit(bins::abs_ const& node) {
    bind_args(a);
    throw NIYError("codegen `abs a`");
  }


  void VisCodegen::visit(bins::add_::Base::Base const& node) {
    ;
  }

  void VisCodegen::visit(bins::add_::Base const& node) {
    replaceSymbol<SyNum, SyNum>([this, node] (SyNum const b_sy) { // need to make a copy because the reference would get overriden
      bind_args(a);
      // if the app typechecks, a /has/ to be a value and not a function
      // which means it does not expect any symbol (which is great because
      // there is no symbol for now)
      // [XXX: in that case, re-introduce NONE, add takeSymbol and placeSymbol]
      this->operator()(a);
      // now there should be a number symbol

      auto a_sy = synum;

      // now we have a_sy and b_sy
      return SyNum(std::string(bins::add_::name) + "$a",
        [=] {
          auto* a = a_sy.make();
          auto* b = b_sy.make();
      // throw BaseError("generates twice the same symbol");
          auto* res = builder.CreateFAdd(a, b, "add_res");
          return res;
        }
      );
    });

    // throw NIYError("codegen `add a`");
    // this->operator()(*node.base);
  }

  void VisCodegen::visit(bins::add_ const& node) {
    throw NIYError("codegen `add a b`");
    bind_args(a, b);
    this->operator()(a);
    this->operator()(b);
    // this->operator()(*node.base);
  }


  // not perfect (eg '1-' is parsed as -1), but will do for now
  void VisCodegen::visit(bins::tonum_::Base const& node) {
    enter(bins::tonum_::name, "");
    replaceSymbol<SyGen, SyNum>([this] (SyGen const& g) { return SyNum(bins::tonum_::name,
      [=] {
        log << "this should be b\n";
        enter(bins::tonum_::name, "value");

        auto* acc = builder.CreateAlloca(num_type, nullptr, "tonum_acc");
        builder.CreateStore(num_val(0), acc);

        auto* isminus = builder.CreateAlloca(bool_type, nullptr, "tonum_isminus");
        builder.CreateStore(bool_false, isminus);
        // auto* isdot = builder.CreateAlloca(bool_type, nullptr, "tonum_isdot");
        // builder.CreateStore(bool_false, isdot);

        g.make(
          builder,
          [=] (BasicBlock* brk, BasicBlock* cont, std::pair<Value*, Value*> ptr_len) {
            enter(bins::tonum_::name, "inject");

            makeBufferLoop("tonum", ptr_len, [&] (Value* at) {
              // _ -> setnegate -> after
              // `-> accumulate ---^
              auto* setnegate = makeBlock(builder, "tonum_setnegate");
              auto* accumulate = makeBlock(builder, "tonum_accumulate");
              auto* after = makeBlock(builder, "tonum_after");

              auto* ch_is_minus = builder.CreateICmpEQ(chr_val('-'), at, "tonum_ch_is_minus");
              auto* l_isminus = builder.CreateLoad(isminus, "tonum_l_isminus");
              auto* not_minus_yet = builder.CreateICmpEQ(bool_false, l_isminus, "tonum_not_minus_yet");
              auto* isnegate = builder.CreateAnd(ch_is_minus, not_minus_yet, "tonum_isnegate");
              builder.CreateCondBr(isnegate, setnegate, accumulate);

              builder.SetInsertPoint(setnegate);
                builder.CreateStore(bool_true, isminus);
                builder.CreateBr(after);

              builder.SetInsertPoint(accumulate);
                auto* digit = builder.CreateSub(at, chr_val('0'), "tonum_digit");

                auto* isdigit_lower = builder.CreateICmpSLE(chr_val(0), digit, "tonum_isdigit_lower");
                auto* isdigit_upper = builder.CreateICmpSLE(digit, chr_val(9), "tonum_isdigit_upper");
                auto* isdigit = builder.CreateAnd(isdigit_lower, isdigit_upper, "tonum_isdigit");

                auto* acc_if_isdigit = makeBlock(builder, "tonum_acc_if_isdigit");
                // break if no longer digit
                builder.CreateCondBr(isdigit, acc_if_isdigit, brk);

                builder.SetInsertPoint(acc_if_isdigit);
                  // acc = acc * 10 + digit
                  auto* xdigit = builder.CreateUIToFP(digit, num_type, "tonum_xdigit");
                  auto* acc_prev = builder.CreateLoad(acc, "tonum_acc_prev");
                  auto* acc_temp = builder.CreateFMul(acc_prev, num_val(10), "tonum_acc_temp");
                  auto* acc_next = builder.CreateFAdd(acc_temp, xdigit, "tonum_acc_next");
                  builder.CreateStore(acc_next, acc);

                builder.CreateBr(after);

              builder.SetInsertPoint(after);
              // continue buffer loop
            });

            // continue generator loop
            builder.CreateBr(cont);

            leave();
          }
        );

        auto* res = builder.CreateLoad(acc, "tonum_res");

        auto* nonegate = builder.GetInsertBlock();
        auto* negate = makeBlock(builder, "tonum_negate");
        auto* retres = makeBlock(builder, "tonum_retres");

        auto* l_isminus = builder.CreateLoad(isminus, "tonum_l_isminus");
        builder.CreateCondBr(l_isminus, negate, retres);

        builder.SetInsertPoint(negate);
          auto* nres = builder.CreateFNeg(res, "tonum_nres");
          builder.CreateBr(retres);

        builder.SetInsertPoint(retres);
          auto* final_res = builder.CreatePHI(num_type, 2, "tonum_final_res");
          final_res->addIncoming(nres, negate);
          final_res->addIncoming(res, nonegate);

        leave();
        return final_res;
      }
    ); });
    leave();
  }

  void VisCodegen::visit(bins::tonum_ const& node) {
    bind_args(a);
    throw NIYError("codegen `tonum a`");
  }


  void VisCodegen::visit(bins::tostr_::Base const& node) {
    enter(bins::tostr_::name, "");
    replaceSymbol<SyNum, SyGen>([this] (SyNum const& n) { return SyGen(bins::tostr_::name,
      [=] {
        enter(bins::tostr_::name, "entry");

        log << "have @tostr generated\n";

        Value* isend = builder.CreateAlloca(bool_type, nullptr, "tostr_isend");
        builder.CreateStore(bool_false, isend);

        auto* b_1char = builder.CreateAlloca(chr_type, len_val(2), "tostr_b_1char");
        auto* at_2nd_char = builder.CreateGEP(b_1char, len_val(1), "tostr_at_2nd_char");

        auto chk = [=] (BasicBlock* brk, BasicBlock* cont) {
          enter(bins::tostr_::name, "check");
          auto* at_isend = builder.CreateLoad(isend, "at_tostr_isend");
          builder.CreateCondBr(at_isend, brk, cont);
          leave();
        };

        auto itr = [=] () {
          enter(bins::tostr_::name, "iter");

          auto* num = n.make();

          log << "call to @tostr, should only be here once\n";
          /*b=*/

          // nominus -> minus -> common
          //     `-----------------^
          auto* nominus = builder.GetInsertBlock();
          auto* minus = makeBlock(builder, "tostr_minus");
          auto* common = makeBlock(builder, "tostr_common");

          auto* isnegative = builder.CreateFCmpOLT(num, num_val(0), "tostr_isnegative");
          builder.CreateCondBr(isnegative, minus, common);

          builder.SetInsertPoint(minus);
            auto* pnum = builder.CreateFNeg(num, "tostr_pnum");
            builder.CreateStore(chr_val('-'), b_1char);
            builder.CreateBr(common);

          builder.SetInsertPoint(common);
            auto* positive = builder.CreatePHI(num_type, 2, "tostr_positive");
            positive->addIncoming(pnum, minus);
            positive->addIncoming(num, nominus);

            // now we also do negative digits!
            auto* store_at = builder.CreatePHI(llvm::Type::getInt8PtrTy(context), 2, "tostr_store_at");
            store_at->addIncoming(at_2nd_char, minus);
            store_at->addIncoming(b_1char, nominus);

            auto* total_len = builder.CreatePHI(len_type, 2, "tostr_total_len");
            total_len->addIncoming(len_val(2), minus);
            total_len->addIncoming(len_val(1), nominus);

            // also for now 1-char buffer, we only do digits here
            auto* inum = builder.CreateFPToUI(positive, len_type, "tostr_temp_inum");
            auto* temp_1xchar = builder.CreateAdd(inum, len_val('0'), "tostr_temp_1xchar");
            auto* temp_1char = builder.CreateTrunc(temp_1xchar, chr_type, "tostr_temp_char");

            builder.CreateStore(temp_1char, store_at);

          // this is the only iteration, mark end
          builder.CreateStore(bool_true, isend);

          leave();
          return std::make_pair(b_1char, total_len);
        };

        leave();
        return std::make_pair(chk, itr);
      }
    ); });
    leave();
  }

  void VisCodegen::visit(bins::tostr_ const& node) {
    bind_args(a);
    throw NIYError("codegen `tostr a`");
  }

} // namespace sel
