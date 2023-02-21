#ifndef _TEMPLLATE_HPP
#define _TEMPLLATE_HPP

#include <llvm/IR/IRBuilder.h>

namespace tll {

  static llvm::IRBuilder<>* __tll_irb;
  inline static void builder(llvm::IRBuilder<>& builder) { __tll_irb = &builder; }
  inline static llvm::IRBuilder<>& builder() { return *__tll_irb; }

  struct letBase {
    llvm::Value* p;
    inline letBase(llvm::Value* p): p(p) { }
    virtual ~letBase() { }
    operator llvm::Value*() const { return p; }
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
    inline let<Ty2> into() const { return llvm_conv<Ty, Ty2>::from(*this); }

    inline static let<Ty*> alloc() { return __tll_irb->CreateAlloca(type(), nullptr); }
    template <typename sz>
    inline static let<Ty*> alloc(let<sz> size) {
      static_assert(std::is_integral<sz>{}, "allocated size should be from an intergal type");
      return __tll_irb->CreateAlloca(type(), size);
    }
    inline static let<Ty*> alloc(int size) { return alloc(let<int>(size)); }

    inline let operator+(let other) const { return llvm_info_for<Ty>::add(*this, other); }
    inline let operator-(let other) const { return llvm_info_for<Ty>::sub(*this, other); }
    inline let operator*(let other) const { return llvm_info_for<Ty>::mul(*this, other); }
    // inline let operator/(let other) const { return llvm_info_for<Ty>::div(*this, other); }

    inline let operator-() const { return let((Ty)0)-*this; }
    inline let& operator+() const { return *this; }

    inline let<bool> operator<(let other) const { return llvm_info_for<Ty>::lt(*this, other); }
    inline let<bool> operator>(let other) const { return llvm_info_for<Ty>::gt(*this, other); }
    inline let<bool> operator<=(let other) const { return llvm_info_for<Ty>::le(*this, other); }
    inline let<bool> operator>=(let other) const { return llvm_info_for<Ty>::ge(*this, other); }
    inline let<bool> operator==(let other) const { return llvm_info_for<Ty>::eq(*this, other); }
    inline let<bool> operator!=(let other) const { return llvm_info_for<Ty>::ne(*this, other); }
  };

  inline let<bool> operator||(let<bool> l, let<bool> r) { return __tll_irb->CreateOr(l, r); }
  inline let<bool> operator&&(let<bool> l, let<bool> r) { return __tll_irb->CreateAnd(l, r); }
  inline let<bool> operator!(let<bool> o) { return __tll_irb->CreateNot(o); }

  template <typename Ty>
  struct letAt {
    let<Ty*> ptr;
    inline letAt(let<Ty*> ptr): ptr(ptr) { }
    template <typename Ty2>
    inline operator let<Ty2>() const { return let<Ty>(__tll_irb->CreateLoad(ptr)); }
    inline void operator=(let<Ty> sto) const { __tll_irb->CreateStore(sto, ptr); }
  };
  template <typename Ty>
  struct letAt<Ty const> {
    let<Ty const*> ptr;
    inline letAt(let<Ty const*> ptr): ptr(ptr) { }
    template <typename Ty2>
    inline operator let<Ty2>() const { return let<Ty>(__tll_irb->CreateLoad(ptr)); }
    inline void operator=(let<Ty const> sto) = delete;
  };

  template <typename Ty>
  struct let<Ty*> : letBase {
    using letBase::letBase;
    inline static llvm::PointerType* type() { return llvm_info_for<typename std::remove_const<Ty>::type>::ptr(); }

    inline operator let<Ty const*>() const { return let<Ty const*>((llvm::Value*)*this); }

    inline letAt<Ty> operator*() const { return letAt<Ty>(*this); }
    template <typename off>
    inline letAt<Ty> operator[](let<off> offset) const { return *((*this) + offset); }
    inline letAt<Ty> operator[](int offset) const { return (*this)[let<int>(offset)]; }

    template <typename off>
    inline let operator+(let<off> offset) const {
      static_assert(std::is_integral<off>{}, "index should be from an intergal type");
      return __tll_irb->CreateGEP(*this, offset);
    }
    inline let operator+(int offset) const { return (*this)+let<int>(offset); }
  };

  template <typename Ret, typename ...Args>
  struct let<Ret(Args...)> : letBase {
    using letBase::letBase;
    inline static llvm::FunctionType* type() { return llvm::FunctionType::get(let<Ret>::type(), {let<Args>::type()...}, false); }

    let(llvm::Value*) = delete;
    inline let(llvm::Twine const& funname, llvm::Module& module, llvm::Function::LinkageTypes linkage)
      : letBase(llvm::Function::Create(type(), linkage, funname, module))
    { }
    inline let(llvm::StringRef funname, llvm::Module& module)
      : letBase(module.getFunction(funname))
    { }
    inline operator llvm::Function*() const { return (llvm::Function*)p; }

    inline let<Ret> operator()(let<Args>... args) const { return __tll_irb->CreateCall(type(), *this, {args...}); }

    inline llvm::BasicBlock* operator[](llvm::Twine const& blockname) const {
      return llvm::BasicBlock::Create(__tll_irb->getContext(), blockname, *this);
    }

    inline void ret(let<Ret> ret) const { __tll_irb->CreateRet(ret); }
    inline void ret(Ret ret) const { __tll_irb->CreateRet(let<Ret>(ret)); }
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

  struct letGlobalConst : letGlobal {
    using letGlobal::letGlobal;
    inline letGlobalConst(llvm::Value*) = delete;

    inline letGlobalConst(llvm::StringRef globname, llvm::Module& module, llvm::GlobalVariable::LinkageTypes linkage, llvm::Constant* v)
      : letGlobal(globname, module, v->getType(), [&] {
          return new llvm::GlobalVariable(
            module, v->getType(), true, // it is constant
            linkage, v, globname
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

    inline let(llvm::StringRef globname, llvm::Module& module, llvm::GlobalVariable::LinkageTypes linkage, std::initializer_list<Has> v)
      : letGlobalConst(globname, module, linkage, llvm::ConstantDataArray::get(__tll_irb->getContext(), v))
    { }

    inline let<Has const*> operator*() const { return (*this)[0]; }
    template <typename off>
    inline let<Has const*> operator[](let<off> offset) const { return __tll_irb->CreateGEP(*this, {let<int>(0), offset}); }
    inline let<Has const*> operator[](int offset) const { return (*this)[(let<int>(offset))]; }

  protected:
    inline let(llvm::StringRef globname, llvm::Module& module, llvm::GlobalVariable::LinkageTypes linkage, llvm::Constant* v)
      : letGlobalConst(globname, module, linkage, v)
    { }
  };

  template <>
  struct let<char const**> : let<std::initializer_list<char>> {
    using let<std::initializer_list<char>>::let;
    let(llvm::Value*) = delete;
    inline static llvm::ArrayType* type(unsigned len) { return llvm::ArrayType::get(let<char>::type(), len); }

    inline let(llvm::StringRef globname, llvm::Module& module, llvm::GlobalVariable::LinkageTypes linkage, llvm::StringRef v, bool addnull=true)
      : let<std::initializer_list<char>>(globname, module, linkage, llvm::ConstantDataArray::getString(__tll_irb->getContext(), v, addnull))
    { }
  };

  template <typename Ty>
  struct letPHI : let<Ty> {
    using let<Ty>::let;
    inline static llvm::Type* type() { return llvm_info_for<Ty>::type(); }

    inline letPHI(std::initializer_list<std::pair<let<Ty>, llvm::BasicBlock*>> vb)
      : let<Ty>(__tll_irb->CreatePHI(let<Ty>::type(), vb.size()))
    { for (auto const& it : vb) ((llvm::PHINode*)this->p)->addIncoming(std::get<0>(it), std::get<1>(it)); }
    inline letPHI(): let<Ty>(__tll_irb->CreatePHI(let<Ty>::type(), 2)) { }

    inline void incoming(let<Ty> v, llvm::BasicBlock* b) { ((llvm::PHINode*)this->p)->addIncoming(v, b); }
  };

  inline llvm::BasicBlock* here() { return __tll_irb->GetInsertBlock(); }
  inline llvm::BasicBlock* block(llvm::Twine const& blockname) { return llvm::BasicBlock::Create(__tll_irb->getContext(), blockname, here()->getParent()); }
  inline void point(llvm::BasicBlock* at) { __tll_irb->SetInsertPoint(at); }

  inline void br(llvm::BasicBlock* to) { __tll_irb->CreateBr(to); }
  inline void cond(let<bool> ifis, llvm::BasicBlock* iftrue, llvm::BasicBlock* iffalse) { __tll_irb->CreateCondBr(ifis, iftrue, iffalse); }

}

#endif // _TEMPLLATE_HPP
