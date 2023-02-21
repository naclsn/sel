#ifndef _TEMPLLATE_HPP
#define _TEMPLLATE_HPP

#include <llvm/IR/IRBuilder.h>

namespace tll {

  static llvm::IRBuilder<>* __tll_irb;
  inline static void builder(llvm::IRBuilder<>& builder) { __tll_irb = &builder; }
  inline static llvm::IRBuilder<>& builder() { return *__tll_irb; }

  struct letBase {
    llvm::Value* p;
    letBase(llvm::Value* p): p(p) { }
    virtual ~letBase() { }
    operator llvm::Value*() { return p; }
  };

  template <typename Ty> struct llvm_info_for;
  template <unsigned n>
  struct llvm_info_for_i {
    inline static llvm::Type* type() { return llvm::Type::getIntNTy(__tll_irb->getContext(), n); }
    inline static llvm::PointerType* ptr() { return llvm::Type::getIntNPtrTy(__tll_irb->getContext(), n); }
    inline static llvm::ConstantInt* val(int v) { return llvm::ConstantInt::get(__tll_irb->getContext(), llvm::APInt(n, v)); }
    inline static llvm::Value* add(llvm::Value* a, llvm::Value* b) { return __tll_irb->CreateAdd(a, b); }
    inline static llvm::Value* sub(llvm::Value* a, llvm::Value* b) { return __tll_irb->CreateSub(a, b); }
    inline static llvm::Value* mul(llvm::Value* a, llvm::Value* b) { return __tll_irb->CreateMul(a, b); }
    // inline static llvm::Value* div(llvm::Value* a, llvm::Value* b) { throw std::runtime_error("TODO: integer division"); }
    inline static llvm::Value* lt(llvm::Value* a, llvm::Value* b) { return __tll_irb->CreateICmpSLT(a, b); }
    inline static llvm::Value* gt(llvm::Value* a, llvm::Value* b) { return __tll_irb->CreateICmpSGT(a, b); }
    inline static llvm::Value* le(llvm::Value* a, llvm::Value* b) { return __tll_irb->CreateICmpSLE(a, b); }
    inline static llvm::Value* ge(llvm::Value* a, llvm::Value* b) { return __tll_irb->CreateICmpSGE(a, b); }
    inline static llvm::Value* eq(llvm::Value* a, llvm::Value* b) { return __tll_irb->CreateICmpEQ(a, b); }
    inline static llvm::Value* ne(llvm::Value* a, llvm::Value* b) { return __tll_irb->CreateICmpNE(a, b); }
  };
  template <> struct llvm_info_for<int> : llvm_info_for_i<32> {};
  template <> struct llvm_info_for<bool> : llvm_info_for_i<1> {};
  template <> struct llvm_info_for<char> : llvm_info_for_i<8> {};
  template <>
  struct llvm_info_for<double> {
    inline static llvm::Type* type() { return llvm::Type::getDoubleTy(__tll_irb->getContext()); }
    inline static llvm::PointerType* ptr() { return llvm::Type::getDoublePtrTy(__tll_irb->getContext()); }
    inline static llvm::ConstantFP* val(double v) { return llvm::ConstantFP::get(__tll_irb->getContext(), llvm::APFloat(v)); }
    inline static llvm::Value* add(llvm::Value* a, llvm::Value* b) { return __tll_irb->CreateFAdd(a, b); }
    inline static llvm::Value* sub(llvm::Value* a, llvm::Value* b) { return __tll_irb->CreateFSub(a, b); }
    inline static llvm::Value* mul(llvm::Value* a, llvm::Value* b) { return __tll_irb->CreateFMul(a, b); }
    // inline static llvm::Value* div(llvm::Value* a, llvm::Value* b) { return __tll_irb->CreateFDiv(a, b); }
    inline static llvm::Value* lt(llvm::Value* a, llvm::Value* b) { return __tll_irb->CreateFCmpOLT(a, b); }
    inline static llvm::Value* gt(llvm::Value* a, llvm::Value* b) { return __tll_irb->CreateFCmpOGT(a, b); }
    inline static llvm::Value* le(llvm::Value* a, llvm::Value* b) { return __tll_irb->CreateFCmpOLE(a, b); }
    inline static llvm::Value* ge(llvm::Value* a, llvm::Value* b) { return __tll_irb->CreateFCmpOGE(a, b); }
    inline static llvm::Value* eq(llvm::Value* a, llvm::Value* b) { return __tll_irb->CreateFCmpOEQ(a, b); }
    inline static llvm::Value* ne(llvm::Value* a, llvm::Value* b) { return __tll_irb->CreateFCmpONE(a, b); }
  };

  // XXX: only signed
  template <typename From, typename To> struct llvm_conv;
  template <unsigned n>
  struct llvm_conv_to_i {
    inline static llvm::Value* from(llvm::Value* v) { return __tll_irb->CreateFPToSI(v, llvm_info_for_i<n>::type()); }
  };
  template <> struct llvm_conv<double, int> : llvm_conv_to_i<32> {};
  template <> struct llvm_conv<double, bool> : llvm_conv_to_i<1> {};
  template <> struct llvm_conv<double, char> : llvm_conv_to_i<8> {};
  template <unsigned n, unsigned n2>
  struct llvm_conv_between_i {
    inline static llvm::Value* from(llvm::Value* v) { return __tll_irb->CreateSExtOrTrunc(v, llvm_info_for_i<n2>::type()); }
  };
  template <> struct llvm_conv<int, bool> : llvm_conv_between_i<32, 1> {};
  template <> struct llvm_conv<int, char> : llvm_conv_between_i<32, 8> {};
  template <> struct llvm_conv<bool, int> : llvm_conv_between_i<1, 32> {};
  template <> struct llvm_conv<bool, char> : llvm_conv_between_i<1, 8> {};
  template <> struct llvm_conv<char, int> : llvm_conv_between_i<8, 32> {};
  template <> struct llvm_conv<char, bool> : llvm_conv_between_i<8, 1> {};
  template <typename From>
  struct llvm_conv<From, double> {
    static_assert(std::is_integral<From>{}, "source of 'to float' conversion does not have an integral type");
    inline static llvm::Value* from(llvm::Value* v) { return __tll_irb->CreateSIToFP(v, llvm_info_for<double>::type()); }
  };

  template <typename Ty>
  struct let : letBase {
    using letBase::letBase;
    inline static llvm::Type* type() { return llvm_info_for<Ty>::type(); }

    inline let(Ty v): letBase(llvm_info_for<Ty>::val(v)) { }
    template <typename Ty2>
    inline let(let<Ty2> other): letBase(llvm_conv<Ty2, Ty>::from(other)) { }

    inline static let<Ty*> alloc() { return __tll_irb->CreateAlloca(type(), nullptr); }
    template <typename sz>
    inline static let<Ty*> alloc(let<sz> size) {
      static_assert(std::is_integral<sz>{}, "allocated size should be from an intergal type");
      return __tll_irb->CreateAlloca(type(), size);
    }
    inline static let<Ty*> alloc(int size) { return alloc(let<int>(size)); }

    inline let operator+(let other) { return llvm_info_for<Ty>::add(*this, other); }
    inline let operator-(let other) { return llvm_info_for<Ty>::sub(*this, other); }
    inline let operator*(let other) { return llvm_info_for<Ty>::mul(*this, other); }
    // inline let operator/(let other) { return llvm_info_for<Ty>::div(*this, other); }

    inline let operator-() { return let(0)-*this; }
    inline let& operator+() { return *this; }

    inline let operator<(let other) { return llvm_info_for<Ty>::lt(*this, other); }
    inline let operator>(let other) { return llvm_info_for<Ty>::gt(*this, other); }
    inline let operator<=(let other) { return llvm_info_for<Ty>::le(*this, other); }
    inline let operator>=(let other) { return llvm_info_for<Ty>::ge(*this, other); }
    inline let operator==(let other) { return llvm_info_for<Ty>::eq(*this, other); }
    inline let operator!=(let other) { return llvm_info_for<Ty>::ne(*this, other); }
  };

  inline let<bool> operator||(let<bool> l, let<bool> r) { return __tll_irb->CreateOr(l, r); }
  inline let<bool> operator&&(let<bool> l, let<bool> r) { return __tll_irb->CreateAnd(l, r); }
  inline let<bool> operator!(let<bool> o) { return __tll_irb->CreateNeg(o); }

  template <typename Ty>
  struct letAt {
    let<Ty*> ptr;
    inline letAt(let<Ty*> ptr): ptr(ptr) { }
    template <typename Ty2>
    inline operator let<Ty2>() { return let<Ty>(__tll_irb->CreateLoad(ptr)); }
    inline void operator=(let<Ty> sto) { __tll_irb->CreateStore(sto, ptr); }
  };
  template <typename Ty>
  struct letAt<Ty const> {
    let<Ty const*> ptr;
    inline letAt(let<Ty const*> ptr): ptr(ptr) { }
    template <typename Ty2>
    inline operator let<Ty2>() { return let<Ty>(__tll_irb->CreateLoad(ptr)); }
    inline void operator=(let<Ty const> sto) = delete;
  };

  template <typename Ty>
  struct let<Ty*> : letBase {
    using letBase::letBase;
    inline static llvm::PointerType* type() { return llvm_info_for<typename std::remove_const<Ty>::type>::ptr(); }

    inline letAt<Ty> operator*() { return letAt<Ty>(*this); }
    template <typename off>
    inline letAt<Ty> operator[](let<off> offset) { return *((*this) + offset); }
    inline letAt<Ty> operator[](int offset) { return (*this)[let<int>(offset)]; }

    template <typename off>
    inline let operator+(let<off> offset) {
      static_assert(std::is_integral<off>{}, "index should be from an intergal type");
      return __tll_irb->CreateGEP(*this, offset);
    }
  };

  // XXX: linkage always external
  template <typename Ret, typename ...Args>
  struct let<Ret(Args...)> : letBase {
    using letBase::letBase;
    inline static llvm::FunctionType* type() { return llvm::FunctionType::get(let<Ret>::type(), {let<Args>::type()...}, false); }

    let(llvm::Value*) = delete;
    inline let(llvm::Twine const& funname, llvm::Module& module)
      : letBase(llvm::Function::Create(type(), llvm::Function::ExternalLinkage, funname, module))
    { }
    inline operator llvm::Function*() { return (llvm::Function*)p; }

    inline let<Ret> operator()(let<Args>... args) { return __tll_irb->CreateCall(type(), *this, {args...}); }

    inline llvm::BasicBlock* operator[](llvm::Twine const& blockname) {
      return llvm::BasicBlock::Create(__tll_irb->getContext(), blockname, *this);
    }

    inline void ret(let<Ret> ret) { __tll_irb->CreateRet(ret); }
    inline void ret(Ret ret) { __tll_irb->CreateRet(let<Ret>(ret)); }
  };

  struct letGlobal : letBase {
    using letBase::letBase;
    inline letGlobal(llvm::Value*) = delete;

    inline letGlobal(llvm::StringRef globname, llvm::Module& module, llvm::Type* type, llvm::function_ref<llvm::GlobalVariable*()> create)
      : letBase(module.getOrInsertGlobal(globname, type, create))
    { }
    inline letGlobal(llvm::StringRef globname, llvm::Module& module)
      : letBase(module.getNamedGlobal(globname))
    { }
  };

  // XXX: linkage always external
  struct letGlobalConst : letGlobal {
    using letGlobal::letGlobal;
    inline letGlobalConst(llvm::Value*) = delete;

    inline letGlobalConst(llvm::StringRef globname, llvm::Module& module, llvm::Type* type, llvm::Constant* v)
      : letGlobal(globname, module, type, [&] {
          return new llvm::GlobalVariable(
            module, type, true, // it is constant
            llvm::GlobalValue::ExternalLinkage,
            v,
            globname
          );
        })
    { }
    inline letGlobalConst(llvm::StringRef globname, llvm::Module& module)
      : letGlobal(globname, module)
    { }
  };

  template <typename Has>
  struct let<std::initializer_list<Has>> : letGlobalConst {
    using letGlobalConst::letGlobalConst;
    let(llvm::Value*) = delete;
    inline static llvm::ArrayType* type(unsigned len) { return llvm::ArrayType::get(let<Has>::type(), len); }

    inline let(llvm::StringRef globname, llvm::Module& module, std::initializer_list<Has> v)
      : letGlobalConst(globname, module, type(v.size()), llvm::ConstantDataArray::get(__tll_irb->getContext(), v))
    { }

    inline let<Has const*> operator*() { return (*this)[0]; }
    template <typename off>
    inline let<Has const*> operator[](let<off> offset) { return __tll_irb->CreateGEP(*this, {let<int>(0), offset}); }
    inline let<Has const*> operator[](int offset) { return (*this)[(let<int>(offset))]; }

  protected:
    inline let(llvm::StringRef globname, llvm::Module& module, llvm::Constant* v)
      : letGlobalConst(globname, module, v->getType(), v)
    { }
  };

  template <>
  struct let<char const**> : let<std::initializer_list<char>> {
    using let<std::initializer_list<char>>::let;
    let(llvm::Value*) = delete;
    inline static llvm::ArrayType* type(unsigned len) { return llvm::ArrayType::get(let<char>::type(), len); }

    inline let(llvm::StringRef globname, llvm::Module& module, llvm::StringRef v, bool addnull=true)
      : let<std::initializer_list<char>>(globname, module, llvm::ConstantDataArray::getString(__tll_irb->getContext(), v, addnull))
    { }
  };

  inline llvm::BasicBlock* here() { return __tll_irb->GetInsertBlock(); }
  inline llvm::BasicBlock* block(llvm::Twine const& blockname) { return llvm::BasicBlock::Create(__tll_irb->getContext(), blockname, here()->getParent()); }
  inline void point(llvm::BasicBlock* at) { __tll_irb->SetInsertPoint(at); }

  inline void br(llvm::BasicBlock* to) { __tll_irb->CreateBr(to); }
  inline void cond(let<bool> ifis, llvm::BasicBlock* iftrue, llvm::BasicBlock* iffalse) { __tll_irb->CreateCondBr(ifis, iftrue, iffalse); }

  template <typename Ty>
  inline let<Ty> phi(std::initializer_list<std::pair<let<Ty>, llvm::BasicBlock*>> b) {
    auto* r = __tll_irb->CreatePHI(let<Ty>::type(), b.size());
    for (auto const& it : b) r->setIncoming(std::get<0>(it), std::get<1>(it));
    return r;
  }
  template <typename Ty>
  inline let<Ty> phi(unsigned n) { return __tll_irb->CreatePHI(let<Ty>::type(), n); }

  // template <typename Ty>
  // inline let<Ty> make(Ty v) { return let<Ty>(v); }
  // template <typename Has>
  // inline let<std::initializer_list<Has>> make(llvm::StringRef globname, llvm::Module& module, std::initializer_list<Has> v) { return let<std::initializer_list<Has>>(globname, module, v); }
  // inline let<char const**> make(llvm::StringRef globname, llvm::Module& module, char const* v) { return let<char const**>(globname, module, v); }

}

#endif // _TEMPLLATE_HPP
