#ifndef _TEMPLLATE_HPP
#define _TEMPLLATE_HPP

#include <llvm/IR/IRBuilder.h>

namespace tll {

  static llvm::IRBuilder<>* __tll_irb;
  inline static void builder(llvm::IRBuilder<>& builder) { __tll_irb = &builder; }
  inline static llvm::IRBuilder<>& builder() { return *__tll_irb; }

  struct letBase {
    llvm::Value* p;
    explicit inline letBase(llvm::Value* p): p(p) { }
    virtual ~letBase() { }
    explicit inline operator llvm::Value*() const { return p; }
  };
  template <typename Ty> struct let;

  template <typename Ty> struct llvm_info_for;
  template <unsigned n, typename Ty>
  struct llvm_info_for_i {
    constexpr static unsigned bits = n;
    inline static llvm::Type* type() { return llvm::Type::getIntNTy(__tll_irb->getContext(), n); }
    inline static llvm::ConstantInt* val(Ty v) { return llvm::ConstantInt::get(__tll_irb->getContext(), llvm::APInt(n, v)); }
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
  template <> struct llvm_info_for<int> : llvm_info_for_i<32, int> {};
  template <> struct llvm_info_for<bool> : llvm_info_for_i<1, bool> {};
  template <> struct llvm_info_for<char> : llvm_info_for_i<8, char> {};
  template <>
  struct llvm_info_for<double> {
    inline static llvm::Type* type() { return llvm::Type::getDoubleTy(__tll_irb->getContext()); }
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
  template <typename Ty>
  struct llvm_info_for<Ty const> {
    inline static llvm::Type* type() { return llvm_info_for<Ty>::type(); }
    inline static llvm::Constant* val(Ty v) { return llvm_info_for<Ty>::val(v); }
  };
  template <typename Ty>
  struct llvm_info_for<Ty*> {
    inline static llvm::PointerType* type() { return llvm_info_for<Ty>::type()->getPointerTo(); }
  };
  template <>
  struct llvm_info_for<void> {
    inline static llvm::Type* type() { return llvm::Type::getVoidTy(__tll_irb->getContext()); }
  };
  template <typename Ret, typename ...Args>
  struct llvm_info_for<Ret(Args...)> {
    inline static llvm::FunctionType* type() { return llvm::FunctionType::get(llvm_info_for<Ret>::type(), {let<Args>::type()...}, false); }
  };

  // XXX: only signed
  template <typename From, typename To> struct llvm_conv;
  template <unsigned n, typename Ty>
  struct llvm_conv_to_i {
    inline static llvm::Value* from(llvm::Value* v) { return __tll_irb->CreateFPToSI(v, llvm_info_for_i<n, Ty>::type()); }
  };
  template <> struct llvm_conv<double, int> : llvm_conv_to_i<32, int> {};
  template <> struct llvm_conv<double, bool> : llvm_conv_to_i<1, bool> {};
  template <> struct llvm_conv<double, char> : llvm_conv_to_i<8, char> {};
  template <unsigned n, unsigned n2, typename Ty2>
  struct llvm_conv_between_i {
    inline static llvm::Value* from(llvm::Value* v) { return __tll_irb->CreateSExtOrTrunc(v, llvm_info_for_i<n2, Ty2>::type()); }
  };
  template <> struct llvm_conv<int, bool> : llvm_conv_between_i<32, 1, bool> {};
  template <> struct llvm_conv<int, char> : llvm_conv_between_i<32, 8, char> {};
  template <> struct llvm_conv<bool, int> : llvm_conv_between_i<1, 32, int> {};
  template <> struct llvm_conv<bool, char> : llvm_conv_between_i<1, 8, char> {};
  template <> struct llvm_conv<char, int> : llvm_conv_between_i<8, 32, int> {};
  template <> struct llvm_conv<char, bool> : llvm_conv_between_i<8, 1, bool> {};
  template <typename From>
  struct llvm_conv<From, double> {
    static_assert(std::is_integral<From>{}, "source of 'to float' conversion does not have an integral type");
    inline static llvm::Value* from(llvm::Value* v) { return __tll_irb->CreateSIToFP(v, llvm_info_for<double>::type()); }
  };
  template <typename From, typename To> struct llvm_conv<From const, To> : llvm_conv<From, To> {};
  template <typename From, typename To> struct llvm_conv<From, To const> : llvm_conv<From, To> {};

  template <typename Ty>
  struct let : letBase {
    using letBase::letBase;
    inline static llvm::Type* type() { return llvm_info_for<Ty>::type(); }

    inline let(Ty v): letBase(llvm_info_for<Ty>::val(v)) { }
    template <typename Ty2>
    inline let<Ty2> into() const { return let<Ty2>(llvm_conv<Ty, Ty2>::from((llvm::Value*)*this)); }

    inline static let<Ty*> alloc() { return let<Ty*>(__tll_irb->CreateAlloca(type(), nullptr)); }
    template <typename sz>
    inline static let<Ty*> alloc(let<sz> size) {
      static_assert(std::is_integral<sz>{}, "allocated size should be from an intergal type");
      return let<Ty*>(__tll_irb->CreateAlloca(type(), (llvm::Value*)size));
    }
    inline static let<Ty*> alloc(int size) { return alloc(let<int>(size)); }

    inline let operator+(let other) const { return let(llvm_info_for<Ty>::add((llvm::Value*)*this, (llvm::Value*)other)); }
    inline let operator-(let other) const { return let(llvm_info_for<Ty>::sub((llvm::Value*)*this, (llvm::Value*)other)); }
    inline let operator*(let other) const { return let(llvm_info_for<Ty>::mul((llvm::Value*)*this, (llvm::Value*)other)); }
    // inline let operator/(let other) const { return let(llvm_info_for<Ty>::div((llvm::Value*)*this, (llvm::Value*)other)); }

    inline let operator-() const { return let((Ty)0)-*this; }
    inline let& operator+() const { return *this; }

    inline let<bool> operator<(let other) const { return let<bool>(llvm_info_for<Ty>::lt((llvm::Value*)*this, (llvm::Value*)other)); }
    inline let<bool> operator>(let other) const { return let<bool>(llvm_info_for<Ty>::gt((llvm::Value*)*this, (llvm::Value*)other)); }
    inline let<bool> operator<=(let other) const { return let<bool>(llvm_info_for<Ty>::le((llvm::Value*)*this, (llvm::Value*)other)); }
    inline let<bool> operator>=(let other) const { return let<bool>(llvm_info_for<Ty>::ge((llvm::Value*)*this, (llvm::Value*)other)); }
    inline let<bool> operator==(let other) const { return let<bool>(llvm_info_for<Ty>::eq((llvm::Value*)*this, (llvm::Value*)other)); }
    inline let<bool> operator!=(let other) const { return let<bool>(llvm_info_for<Ty>::ne((llvm::Value*)*this, (llvm::Value*)other)); }
  };

  inline let<bool> operator||(let<bool> l, let<bool> r) { return let<bool>(__tll_irb->CreateOr((llvm::Value*)l, (llvm::Value*)r)); }
  inline let<bool> operator&&(let<bool> l, let<bool> r) { return let<bool>(__tll_irb->CreateAnd((llvm::Value*)l, (llvm::Value*)r)); }
  inline let<bool> operator!(let<bool> o) { return let<bool>(__tll_irb->CreateNot((llvm::Value*)o)); }

  template <typename Ty>
  struct letAt {
    inline static llvm::FunctionType* type() { return let<Ty>::type(); }
    let<Ty*> ptr;
    inline letAt(let<Ty*> ptr): ptr(ptr) { }
    inline operator let<Ty>() const { return let<Ty>(__tll_irb->CreateLoad((llvm::Value*)ptr)); }
    inline void operator=(let<Ty> sto) const { __tll_irb->CreateStore((llvm::Value*)sto, (llvm::Value*)ptr); }
  };
  template <typename Ty>
  struct letAt<Ty const> {
    inline static llvm::FunctionType* type() { return let<Ty>::type(); }
    let<Ty const*> ptr;
    inline letAt(let<Ty const*> ptr): ptr(ptr) { }
    inline operator let<Ty>() const { return let<Ty>(__tll_irb->CreateLoad((llvm::Value*)ptr)); }
    inline operator let<Ty const>() const { return let<Ty const>(__tll_irb->CreateLoad((llvm::Value*)ptr)); }
    inline void operator=(let<Ty const> sto) = delete;
  };
  template <typename Ret, typename ...Args>
  struct letAt<Ret(Args...)> {
    inline static llvm::FunctionType* type() { return let<Ret(Args...)>::type(); }
    let<Ret(*)(Args...)> ptr;
    inline letAt(let<Ret(*)(Args...)> ptr): ptr(ptr) { }
    template <typename Ret2, typename ...Args2>
    inline void operator=(let<Ret(Args...)> sto) = delete;
    inline let<Ret> operator()(let<Args>... args) const { return let<Ret>(__tll_irb->CreateCall(type(), (llvm::Value*)ptr, {(llvm::Value*)args...})); }
  };
  template <typename ...Args>
  struct letAt<void(Args...)> {
    inline static llvm::FunctionType* type() { return let<void(Args...)>::type(); }
    let<void(*)(Args...)> ptr;
    inline letAt(let<void(*)(Args...)> ptr): ptr(ptr) { }
    template <typename ...Args2>
    inline void operator=(let<void(Args...)> sto) = delete;
    inline void operator()(let<Args>... args) const { __tll_irb->CreateCall(type(), (llvm::Value*)ptr, {(llvm::Value*)args...}); }
  };

  template <typename Ty>
  struct let<Ty*> : letBase {
    using letBase::letBase;
    inline static llvm::PointerType* type() { return llvm_info_for<Ty*>::type(); }

    inline operator let<Ty const*>() const { return let<Ty const*>((llvm::Value*)*this); }

    inline static let<Ty**> alloc() { return let<Ty**>(__tll_irb->CreateAlloca(type(), nullptr)); }
    template <typename sz>
    inline static let<Ty**> alloc(let<sz> size) {
      static_assert(std::is_integral<sz>{}, "allocated size should be from an intergal type");
      return let<Ty**>(__tll_irb->CreateAlloca(type(), (llvm::Value*)size));
    }
    inline static let<Ty**> alloc(int size) { return alloc(let<int>(size)); }

    inline letAt<Ty> operator*() const { return letAt<Ty>(*this); }
    template <typename off>
    inline letAt<Ty> operator[](let<off> offset) const { return *((*this) + offset); }
    inline letAt<Ty> operator[](int offset) const { return (*this)[let<int>(offset)]; }

    template <typename off>
    inline let operator+(let<off> offset) const {
      static_assert(std::is_integral<off>{}, "index should be from an intergal type");
      return let(__tll_irb->CreateGEP((llvm::Value*)*this, (llvm::Value*)offset));
    }
    inline let operator+(int offset) const { return (*this)+let<int>(offset); }
  };

  template <typename Ret, typename ...Args>
  struct let<Ret(Args...)> : letBase {
    using letBase::letBase;
    inline static llvm::FunctionType* type() { return llvm_info_for<Ret(Args...)>::type(); }

    let(llvm::Value*) = delete;
    inline let(llvm::StringRef funname, llvm::Module& module, llvm::Function::LinkageTypes linkage)
      : letBase(module.getFunction(funname)
          ? module.getFunction(funname)
          : llvm::Function::Create(type(), linkage, funname, module)
        )
    { }
    inline let(llvm::StringRef funname, llvm::Module& module)
      : letBase(module.getFunction(funname))
    { }
    explicit inline operator llvm::Function*() const { return (llvm::Function*)p; }

    inline let<Ret> operator()(let<Args>... args) const { return let<Ret>(__tll_irb->CreateCall(type(), (llvm::Value*)*this, {(llvm::Value*)args...})); }
    inline let<Ret(*)(Args...)> operator*() const { return let<Ret(*)(Args...)>(p); }

    llvm::BasicBlock* e = nullptr;
    inline llvm::BasicBlock* entry() {
      if (e) return e;
      return e = llvm::BasicBlock::Create(__tll_irb->getContext(), "entry", (llvm::Function*)*this);
    }

    // see `get<n>(f)` to get proper `let<>` typing
    inline llvm::Argument* operator[](int argn) const {
      auto a = ((llvm::Function*)p)->args();
      return a.begin() + argn;
    }

    inline void ret(let<Ret> ret) const { __tll_irb->CreateRet((llvm::Value*)ret); }
  };

  template <typename ...Args>
  struct let<void(Args...)> : letBase {
    using letBase::letBase;
    inline static llvm::FunctionType* type() { return llvm_info_for<void(Args...)>::type(); }

    let(llvm::Value*) = delete;
    inline let(llvm::StringRef funname, llvm::Module& module, llvm::Function::LinkageTypes linkage)
      : letBase(module.getFunction(funname)
          ? module.getFunction(funname)
          : llvm::Function::Create(type(), linkage, funname, module)
        )
    { }
    inline let(llvm::StringRef funname, llvm::Module& module)
      : letBase(module.getFunction(funname))
    { }
    explicit inline operator llvm::Function*() const { return (llvm::Function*)p; }

    inline void operator()(let<Args>... args) const { __tll_irb->CreateCall(type(), (llvm::Value*)*this, {(llvm::Value*)args...}); }
    inline let<void(*)(Args...)> operator*() const { return let<void(*)(Args...)>(p); }

    llvm::BasicBlock* e = nullptr;
    inline llvm::BasicBlock* entry() {
      if (e) return e;
      return e = llvm::BasicBlock::Create(__tll_irb->getContext(), "entry", (llvm::Function*)*this);
    }

    // see `get<n>(f)` to get proper `let<>` typing
    inline llvm::Argument* operator[](int argn) const {
      auto a = ((llvm::Function*)p)->args();
      return a.begin() + argn;
    }

    inline void ret() const { __tll_irb->CreateRetVoid(); }
  };

  template <size_t argn, typename Ret, typename ...Args>
  inline let<typename std::tuple_element<argn, std::tuple<Args...>>::type> get(let<Ret(Args...)> f) {
    return let<typename std::tuple_element<argn, std::tuple<Args...>>::type>(f[argn]);
  }

  template <typename Ty> struct letGlobal;

  template <typename Ty>
  struct letGlobal<Ty*> : letBase {
    using letBase::letBase;
    letGlobal(llvm::Value*) = delete;
    inline static llvm::Type* type() { return let<Ty*>::type(); }

    inline letGlobal(llvm::StringRef globname, llvm::Module& module)
      : letBase(module.getNamedGlobal(globname))
    { }
    inline letGlobal(llvm::StringRef globname, llvm::Module& module, llvm::GlobalVariable::LinkageTypes linkage, Ty v)
      : letGlobal(globname, module, linkage, llvm_info_for<Ty>::val(v))
    { }

    inline operator let<Ty*>() const { return *this; }
    // inline let<Ty> operator*() const { return __tll_irb->CreateGEP(*this, let<int>(0)); }
    inline letAt<Ty> operator*() const { return letAt<Ty>(__tll_irb->CreateGEP((llvm::Value*)*this, (llvm::Value*)let<int>(0))); }

  protected:
    inline letGlobal(llvm::StringRef globname, llvm::Module& module, llvm::GlobalVariable::LinkageTypes linkage, llvm::Constant* v)
      : letBase(module.getOrInsertGlobal(globname, v->getType(), [&] {
          return new llvm::GlobalVariable(module, v->getType(), std::is_const<Ty>::value, linkage, v, globname);
        }))
    { }
  };

  template <typename Ty>
  struct letGlobal<Ty**> : letBase {
    using letBase::letBase;
    letGlobal(llvm::Value*) = delete;
    inline static llvm::ArrayType* type(unsigned len) { return llvm::ArrayType::get(let<Ty>::type(), len); }

    inline letGlobal(llvm::StringRef globname, llvm::Module& module)
      : letBase(module.getNamedGlobal(globname))
    { }
    inline letGlobal(llvm::StringRef globname, llvm::Module& module, llvm::GlobalVariable::LinkageTypes linkage, llvm::ArrayRef<Ty> v)
      : letGlobal(globname, module, linkage, llvm::ConstantDataArray::get(__tll_irb->getContext(), v))
    { }
    inline letGlobal(llvm::StringRef globname, llvm::Module& module, llvm::GlobalVariable::LinkageTypes linkage, llvm::StringRef v, bool addnull=true)
      : letGlobal(globname, module, linkage, llvm::ConstantDataArray::getString(__tll_irb->getContext(), v, addnull))
    { static_assert(std::is_same<char, typename std::remove_const<Ty>::type>{}, "only char** or char const** can be initialized with a string"); }

    inline let<Ty*> operator*() const { return (*this)[0]; }
    template <typename off>
    inline let<Ty*> operator[](let<off> offset) const { return let<Ty*>(__tll_irb->CreateGEP((llvm::Value*)*this, {(llvm::Value*)let<int>(0), (llvm::Value*)offset})); }
    inline let<Ty*> operator[](int offset) const { return (*this)[(let<int>(offset))]; }

  protected:
    inline letGlobal(llvm::StringRef globname, llvm::Module& module, llvm::GlobalVariable::LinkageTypes linkage, llvm::Constant* v)
      : letBase(module.getOrInsertGlobal(globname, v->getType(), [&] {
          return new llvm::GlobalVariable(module, v->getType(), std::is_const<Ty>::value, linkage, v, globname);
        }))
    { }
  };

  template <typename Ty>
  struct letPHI : let<Ty> {
    using let<Ty>::let;
    inline static llvm::Type* type() { return llvm_info_for<Ty>::type(); }

    inline letPHI(std::initializer_list<std::pair<let<Ty>, llvm::BasicBlock*>> vb)
      : let<Ty>(__tll_irb->CreatePHI(let<Ty>::type(), vb.size()))
    { for (auto const& it : vb) ((llvm::PHINode*)*this)->addIncoming((llvm::Value*)std::get<0>(it), std::get<1>(it)); }
    inline letPHI(): let<Ty>(__tll_irb->CreatePHI(let<Ty>::type(), 2)) { }
    explicit inline operator llvm::PHINode*() const { return (llvm::PHINode*)letBase::p; }

    inline void incoming(let<Ty> v, llvm::BasicBlock* b) { ((llvm::PHINode*)*this)->addIncoming((llvm::Value*)v, b); }
  };

  inline llvm::BasicBlock* here() { return __tll_irb->GetInsertBlock(); }
  inline llvm::BasicBlock* block(llvm::Twine const& blockname) { return llvm::BasicBlock::Create(__tll_irb->getContext(), blockname, here()->getParent()); }
  inline void point(llvm::BasicBlock* at) { __tll_irb->SetInsertPoint(at); }

  inline void br(llvm::BasicBlock* to) { __tll_irb->CreateBr(to); }
  inline void cond(let<bool> ifis, llvm::BasicBlock* iftrue, llvm::BasicBlock* iffalse) { __tll_irb->CreateCondBr((llvm::Value*)ifis, iftrue, iffalse); }

}

#endif // _TEMPLLATE_HPP
