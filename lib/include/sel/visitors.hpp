#ifndef SEL_VISITORS_HPP
#define SEL_VISITORS_HPP

/**
 * Setup for "visitor pattern" over every derivatives of
 * `Val`. Also defines a few such as `ValRepr`. Note that
 * this pattern may be quite heavy and is not meant to be
 * implemented where performance matters.
 */

#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>
#include <functional>
#include <stack>
#include <list>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_os_ostream.h>

#include "types.hpp"
#include "builtins.hpp"

namespace sel {

  class VisitorLits {
  private:
    void ni(char const* waai) {
      std::ostringstream oss;
      throw NIYError((oss << "operation not supported for this visitor, on " << waai, oss.str()));
    }
  public:
    virtual ~VisitorLits() { }
    virtual void visitNumLiteral(Type const& type, double n) { ni("NumLiteral"); }
    virtual void visitStrLiteral(Type const& type, std::string const& s) { ni("StrLiteral"); }
    virtual void visitLstLiteral(Type const& type, std::vector<Val*> const& v) { ni("LstLiteral"); }
    virtual void visitFunChain(Type const& type, std::vector<Fun*> const& f) { ni("FunChain"); }
    virtual void visitStrChunks(Type const& type, std::vector<std::string> const& vs) { ni("StrChunks"); }
    virtual void visitLstMapCoerse(Type const& type, Lst const& l) { ni("LstMapCoerse"); }
    virtual void visitInput(Type const& type) { ni("Input"); }
  };

  template <typename L>
  class _make_BinsVisitorBase : public _make_BinsVisitorBase<typename L::cdr> {
  public:
    using _make_BinsVisitorBase<typename L::cdr>::visit;
    virtual void visit(typename L::car const& val) {
      std::ostringstream oss;
      throw NIYError((oss << "operation not supported for this visitor, on this value of type " << val.type(), oss.str()));
    }
  };
  template <>
  class _make_BinsVisitorBase<bins_ll::nil> {
  public:
    virtual ~_make_BinsVisitorBase() { }
    // YYY: this template fallback makes it possible to
    // comment out entries in the `bins_ll::bins` list
    template <typename T> void visit(T const&) { }
  };

  typedef _make_BinsVisitorBase<bins_ll::bins> VisitorBins;
  typedef _make_BinsVisitorBase<bins_ll::bins_all> VisitorBinsAll;

  /**
   * Base class for the visitor pattern over every `Val`.
   */
  class Visitor : public VisitorLits, public VisitorBinsAll {
  public:
    void operator()(Val const& val);
  };


  struct _ReprCx {
    unsigned indents;
    bool top_level;
    bool single_line;
  };

  template <typename L>
  class _VisRepr : public _VisRepr<typename L::cdr> {
  private:
    typedef typename L::car _b;
  protected:
    _VisRepr(std::ostream& res, _ReprCx cx): _VisRepr<typename L::cdr>(res, cx) { }
  public:
    using _VisRepr<typename L::cdr>::visit;
    void visit(_b const& it) override {
      this->visitCommon(it, typename std::conditional<!_b::args, std::true_type, std::false_type>::type{});
    }
  };

  template <>
  class _VisRepr<bins_ll::nil> : public Visitor {
  protected:
    struct ReprField {
      char const* name;
      enum { DBL, STR, VAL } const data_ty;
      union { double const dbl; std::string const* str; Val const* val; } const data;
    };
    _VisRepr(std::ostream& res, _ReprCx cx): res(res), cx(cx) { }
    std::ostream& res;
    _ReprCx cx;
    void reprHelper(Type const& type, char const* name, std::vector<ReprField> const fields);
    template <typename T>
    void visitCommon(T const&, std::false_type is_head);
    template <typename T>
    void visitCommon(T const&, std::true_type is_head);
  };

  class VisRepr : public _VisRepr<bins_ll::bins_all> {
  public:
    typedef _ReprCx ReprCx;
    VisRepr(std::ostream& res, ReprCx cx={.top_level=true}): _VisRepr(res, cx) { }
    void visitNumLiteral(Type const& type, double n) override;
    void visitStrLiteral(Type const& type, std::string const& s) override;
    void visitLstLiteral(Type const& type, std::vector<Val*> const& v) override;
    void visitStrChunks(Type const& type, std::vector<std::string> const& vs) override;
    void visitLstMapCoerse(Type const& type, Lst const& l) override;
    void visitFunChain(Type const& type, std::vector<Fun*> const& f) override;
    void visitInput(Type const& type) override;
  };


  template <typename L>
  class _VisHelp : public _VisHelp<typename L::cdr> {
  protected:
    _VisHelp(std::ostream& res): _VisHelp<typename L::cdr>(res) { }
  public:
    using _VisHelp<typename L::cdr>::visit;
    void visit(typename L::car::Head const& it) override {
      this->res << std::remove_reference<decltype(it)>::type::the::Base::Next::doc;
    }
  };

  template <>
  class _VisHelp<bins_ll::nil> : public Visitor {
  protected:
    _VisHelp(std::ostream& res): res(res) { }
    std::ostream& res;
  };

  class VisHelp : public _VisHelp<bins_ll::bins> {
  public:
    VisHelp(std::ostream& res): _VisHelp(res) { }
  };

  class VisCodegen : public Visitor {
    llvm::LLVMContext context;
    llvm::IRBuilder<> builder;
    llvm::Module module;

    indented& log;

    // things that can come out of SyGen, passed to the injection `also(brk, cont, <>)`
    struct Generated {
      union {
        llvm::Value* num;
        struct { llvm::Value* ptr; llvm::Value* len; };
      };
      Generated(llvm::Value* num): num(num) { }
      Generated(llvm::Value* ptr, llvm::Value* len): ptr(ptr), len(len) { }
    };

    //{{{ some codegen utils

    llvm::BasicBlock* makeBlock(std::string const name);

    // note that the `body` function gets the bytes pointed at,
    // not the iteration variable (ie a chr, not chr* nor len)
    void makeBufferLoop(std::string const name
      , llvm::Value* ptr
      , llvm::Value* len
      , std::function<void(llvm::BasicBlock* brk, llvm::BasicBlock* cont, llvm::Value* at)> body
      );

    void makeStream(std::string const name
      , std::function<void(llvm::BasicBlock* brk, llvm::BasicBlock* cont)> chk
      , std::function<Generated()> itr
      , std::function<void(llvm::BasicBlock* brk, llvm::BasicBlock* cont, Generated it)> also
      );

    //}}} some codegen utils

    //{{{ symbol shenanigan

    /// the injection (sometimes `also`) must branch to exit or iter
    /// in the case of a SyNum, brk and cont are both the same
    typedef std::function
      < void
        (llvm::BasicBlock* brk, llvm::BasicBlock* cont, Generated it)
      > inject_clo_type;
    typedef std::function<void(inject_clo_type)> clo_type;

    struct Symbol {
      std::string name;
      clo_type doobidoo;

      Symbol(std::string const name, clo_type doobidoo)
        : name(name)
        , doobidoo(doobidoo)
      { }
      virtual ~Symbol() { }

      virtual void make(inject_clo_type also) const { doobidoo(also); }
    };

    std::stack<Symbol, std::list<Symbol>> systack;
    void place(std::string const, clo_type doobidoo);
    Symbol const take();

    void invoke(Val const&, int);

    //}}} symbol shenanigan

    //{{{ llvm types and values

    llvm::Type* bool_type;
    llvm::Constant* bool_true;
    llvm::Constant* bool_false;
    /// double -- type (and constant values) for Num
    llvm::Type* num_type;
    inline llvm::Constant* num_val(double v) { return llvm::ConstantFP::get(context, llvm::APFloat(v)); }
    /// i8 -- types (and constant values) for the contained units in Str
    llvm::Type* chr_type;
    llvm::Type* ptr_type;
    inline llvm::Constant* chr_val(int8_t v) { return llvm::ConstantInt::get(context, llvm::APInt(8, v)); }
    /// i32 -- type (and constant values) for the counters and lengths (so for Str/Lst)
    llvm::Type* len_type;
    inline llvm::Constant* len_val(size_t v) { return llvm::ConstantInt::get(context, llvm::APInt(/*64*/32, v)); }

    //}}} llvm types and values

  public:
    ~VisCodegen() { delete &log; }
    // entry, preambule
    VisCodegen(char const* file_name, char const* module_name, App& app);
    // exit, post-thingy
    void* makeOutput();

    void dump(std::ostream& out) const {
      llvm::raw_os_ostream o(out);
      module.print(o, nullptr);
    }
    void dothething(char const* outfile);

    void visitNumLiteral(Type const& type, double n) override;
    void visitStrLiteral(Type const& type, std::string const& s) override;
    void visitLstLiteral(Type const& type, std::vector<Val*> const& v) override;
    void visitStrChunks(Type const& type, std::vector<std::string> const& vs) override;
    void visitFunChain(Type const& type, std::vector<Fun*> const& f) override;
    void visitInput(Type const& type) override;

    void visit(bins::abs_::Base const&) override;
    void visit(bins::abs_ const&) override;
    void visit(bins::add_::Base::Base const&) override;
    void visit(bins::add_::Base const&) override;
    void visit(bins::add_ const&) override;
    void visit(bins::bytes_::Base const&) override;
    void visit(bins::bytes_ const&) override;
    void visit(bins::id_ const&) override;
    void visit(bins::map_::Base::Base const&) override;
    void visit(bins::map_::Base const&) override;
    void visit(bins::map_ const&) override;
    void visit(bins::sub_::Base::Base const&) override;
    void visit(bins::sub_::Base const&) override;
    void visit(bins::sub_ const&) override;
    void visit(bins::tonum_::Base const&) override;
    void visit(bins::tonum_ const&) override;
    void visit(bins::tostr_::Base const&) override;
    void visit(bins::tostr_ const&) override;
    void visit(bins::unbytes_::Base const&) override;
    void visit(bins::unbytes_ const&) override;
  };

} // namespace sel

#endif // SEL_VISITORS_HPP
