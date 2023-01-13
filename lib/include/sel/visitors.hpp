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
#include <string>
#include <vector>

#include "types.hpp"
#include "builtins.hpp"

namespace sel {

  class Val;
  class Fun; // YYY: for FunChain, but I would prefer without

  class VisitorLits {
  private:
    void ni(char const* waai) { std::cerr << "This visitor does not handle '" << waai << "'\n"; } // YYY: proper throw?
  public:
    virtual ~VisitorLits() { }
    virtual void visitNumLiteral(Type const& type, double n) { ni("NumLiteral"); }
    virtual void visitStrLiteral(Type const& type, std::string const& s) { ni("StrLiteral"); }
    virtual void visitLstLiteral(Type const& type, std::vector<Val*> const& v) { ni("LstLiteral"); }
    virtual void visitFunChain(Type const& type, std::vector<Fun*> const& f) { ni("FunChain"); }
    virtual void visitStrChunks(Type const& type, std::vector<std::string> const& vs) { ni("StrChunks"); }
  //class VisitorSpec
    virtual void visitInput(Type const& type) { ni("Input"); }
    virtual void visitOutput(Type const& type) { ni("Output"); }
  };

  template <typename L>
  class _make_BinsVisitorBase : public _make_BinsVisitorBase<typename L::cdr> {
  public:
    using _make_BinsVisitorBase<typename L::cdr>::visit;
    virtual void visit(typename L::car const& val) {
      // was TypeError, could be RuntimeError or even simply BaseError...
      // throw NIYError(std::string("operation not supported: ") + typeid(*this).name() + "(" + typeid(val).name() + ")");
      throw NIYError("operation not supported for this visitor, on this value");
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


    // ZZZ: outside, dont like, pollutes
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

    // TODO/ZZZ: special (jank) cases
    template <typename cdr>
    class _VisRepr<bins_ll::cons<bins::const_, cdr>> : public _VisRepr<cdr> {
    private:
      typedef bins::const_ _b;
    protected:
      _VisRepr(std::ostream& res, _ReprCx cx): _VisRepr<cdr>(res, cx) { }
    public:
      using _VisRepr<cdr>::visit;
      void visit(_b const& it) override {
        this->visitCommon(it._base, typename std::conditional<!_b::args, std::true_type, std::false_type>::type{});
      }
    };
    template <typename cdr>
    class _VisRepr<bins_ll::cons<bins::flip_, cdr>> : public _VisRepr<cdr> {
    private:
      typedef bins::flip_ _b;
    protected:
      _VisRepr(std::ostream& res, _ReprCx cx): _VisRepr<cdr>(res, cx) { }
    public:
      using _VisRepr<cdr>::visit;
      void visit(_b const& it) override {
        this->visitCommon(it._base, typename std::conditional<!_b::args, std::true_type, std::false_type>::type{});
      }
    };
    // special-SPECIAL case (even more jank) of `id_`
    template <typename cdr>
    class _VisRepr<bins_ll::cons<bins::if_, cdr>> : public _VisRepr<cdr> {
    private:
      typedef bins::if_ _b;
    protected:
      _VisRepr(std::ostream& res, _ReprCx cx): _VisRepr<cdr>(res, cx) { }
    public:
      using _VisRepr<cdr>::visit;
      void visit(_b const& it) override {
        this->visitCommon(it._base, typename std::conditional<!_b::args, std::true_type, std::false_type>::type{});
      }
    };
    template <typename cdr>
    class _VisRepr<bins_ll::cons<bins::uncurry_, cdr>> : public _VisRepr<cdr> {
    private:
      typedef bins::uncurry_ _b;
    protected:
      _VisRepr(std::ostream& res, _ReprCx cx): _VisRepr<cdr>(res, cx) { }
    public:
      using _VisRepr<cdr>::visit;
      void visit(_b const& it) override {
        this->visitCommon(it._base, typename std::conditional<!_b::args, std::true_type, std::false_type>::type{});
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
    void visitFunChain(Type const& type, std::vector<Fun*> const& f) override;
    void visitInput(Type const& type) override;
    void visitOutput(Type const& type) override;
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

} // namespace sel

#endif // SEL_VISITORS_HPP
