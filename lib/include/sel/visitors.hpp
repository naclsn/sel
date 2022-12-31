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
#include <typeinfo>

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
      throw TypeError(std::string("operation not supported: ") + typeid(*this).name() + "(" + typeid(val).name() + ")");
      // throw NIYError(std::string("'visit' of visitor pattern for this class: ") + typeid(val).name() + " (visitor class: " + typeid(*this).name() + ")");
    }
  };
  template <>
  class _make_BinsVisitorBase<bins_ll::nil> {
  public:
    virtual ~_make_BinsVisitorBase() { }
    void visit() { } // YYY: because the 'using' statement (to silence warnings) needs it
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

  // class VisRepr : public Visitor {
  // public:
  //   struct ReprCx {
  //     unsigned indents;
  //     bool top_level;
  //     bool single_line;
  //   };
  //   VisRepr(std::ostream& res, ReprCx cx={.top_level=true})
  //     : res(res)
  //     , cx(cx)
  //   { }

  // private:
  //   std::ostream& res;
  //   struct ReprField {
  //     char const* name;
  //     enum {
  //       DBL,
  //       STR,
  //       VAL,
  //     } const data_ty;
  //     union {
  //       Val const* val;
  //       std::string const* str;
  //       double const dbl;
  //     } const data;
  //   };
  //   ReprCx cx;

  //   void reprHelper(Type const& type, char const* name, std::vector<ReprField> const fields);

  //   template <typename T>
  //   void visitCommon(T const&, std::false_type is_head);
  //   template <typename T>
  //   void visitCommon(T const&, std::true_type is_head);

  // public:
  //   void visitNumLiteral(Type const& type, double n) override;
  //   void visitStrLiteral(Type const& type, std::string const& s) override;
  //   void visitLstLiteral(Type const& type, std::vector<Val*> const& v) override;
  //   void visitStrChunks(Type const& type, std::vector<std::string> const& vs) override;
  //   void visitFunChain(Type const& type, std::vector<Fun*> const& f) override;
  //   void visitInput(Type const& type) override;
  //   void visitOutput(Type const& type) override;

  //   // XXX: would love any form of solution to this, but there might no be any du to the nature of the problem
  //   void visit(bins::abs_ const&) override;
  //   void visit(bins::abs_::Base const&) override;
  //   // void visit(bins::add_ const&) override;
  //   // void visit(bins::add_::Base const&) override;
  //   // void visit(bins::add_::Base::Base const&) override;
  //   // void visit(bins::conjunction_ const&) override;
  //   // void visit(bins::conjunction_::Base const&) override;
  //   // void visit(bins::conjunction_::Base::Base const&) override;
  //   // void visit(bins::const_ const&) override;
  //   // void visit(bins::const_::Base const&) override;
  //   // void visit(bins::drop_ const&) override;
  //   // void visit(bins::drop_::Base const&) override;
  //   // void visit(bins::drop_::Base::Base const&) override;
  //   // void visit(bins::dropwhile_ const&) override;
  //   // void visit(bins::dropwhile_::Base const&) override;
  //   // void visit(bins::dropwhile_::Base::Base const&) override;
  //   // void visit(bins::filter_ const&) override;
  //   // void visit(bins::filter_::Base const&) override;
  //   // void visit(bins::filter_::Base::Base const&) override;
  //   // void visit(bins::flip_ const&) override;
  //   // void visit(bins::flip_::Base const&) override;
  //   // void visit(bins::flip_::Base::Base const&) override;
  //   // void visit(bins::id_ const&) override;
  //   // void visit(bins::if_ const&) override;
  //   // void visit(bins::if_::Base const&) override;
  //   // void visit(bins::if_::Base::Base const&) override;
  //   // void visit(bins::if_::Base::Base::Base const&) override;
  //   // void visit(bins::iterate_ const&) override;
  //   // void visit(bins::iterate_::Base const&) override;
  //   // void visit(bins::iterate_::Base::Base const&) override;
  //   // void visit(bins::join_ const&) override;
  //   // void visit(bins::join_::Base const&) override;
  //   // void visit(bins::join_::Base::Base const&) override;
  //   // void visit(bins::map_ const&) override;
  //   // void visit(bins::map_::Base const&) override;
  //   // void visit(bins::map_::Base::Base const&) override;
  //   // void visit(bins::nl_ const&) override;
  //   // void visit(bins::nl_::Base const&) override;
  //   // void visit(bins::pi_ const&) override;
  //   // void visit(bins::repeat_ const&) override;
  //   // void visit(bins::repeat_::Base const&) override;
  //   // void visit(bins::replicate_ const&) override;
  //   // void visit(bins::replicate_::Base const&) override;
  //   // void visit(bins::replicate_::Base::Base const&) override;
  //   // void visit(bins::reverse_ const&) override;
  //   // void visit(bins::reverse_::Base const&) override;
  //   // void visit(bins::singleton_ const&) override;
  //   // void visit(bins::singleton_::Base const&) override;
  //   // void visit(bins::split_ const&) override;
  //   // void visit(bins::split_::Base const&) override;
  //   // void visit(bins::split_::Base::Base const&) override;
  //   // void visit(bins::sub_ const&) override;
  //   // void visit(bins::sub_::Base const&) override;
  //   // void visit(bins::sub_::Base::Base const&) override;
  //   // void visit(bins::take_ const&) override;
  //   // void visit(bins::take_::Base const&) override;
  //   // void visit(bins::take_::Base::Base const&) override;
  //   // void visit(bins::takewhile_ const&) override;
  //   // void visit(bins::takewhile_::Base const&) override;
  //   // void visit(bins::takewhile_::Base::Base const&) override;
  //   // void visit(bins::tonum_ const&) override;
  //   // void visit(bins::tonum_::Base const&) override;
  //   // void visit(bins::tostr_ const&) override;
  //   // void visit(bins::tostr_::Base const&) override;
  //   // void visit(bins::uncurry_ const&) override;
  //   // void visit(bins::uncurry_::Base const&) override;
  //   // void visit(bins::zipwith_ const&) override;
  //   // void visit(bins::zipwith_::Base const&) override;
  //   // void visit(bins::zipwith_::Base::Base const&) override;
  //   // void visit(bins::zipwith_::Base::Base::Base const&) override;
  // };

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
  //   using Visitor::visit;
  };

} // namespace sel

#endif // SEL_VISITORS_HPP
