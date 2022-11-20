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
  public:
    virtual ~VisitorLits() { }
    virtual void visitNumLiteral(Type const& type, double n) = 0;
    virtual void visitStrLiteral(Type const& type, std::string const& s) = 0;
    virtual void visitLstLiteral(Type const& type, std::vector<Val*> const& v) = 0;
    virtual void visitFunChain(Type const& type, std::vector<Fun*> const& f) = 0;
  //class VisitorSpec
    virtual void visitInput(Type const& type) = 0;
    virtual void visitOutput(Type const& type) = 0;
  };

  template <typename L>
  class _make_BinsVisitorBase : public _make_BinsVisitorBase<typename L::cdr> {
  public:
    using _make_BinsVisitorBase<typename L::cdr>::visit;
    virtual void visit(typename L::car const& val) {
      std::cerr << typeid(*this).name() << ": NIY: visiting " << typeid(val).name() << '\n'; // ZZZ
    }
  };
  template <>
  class _make_BinsVisitorBase<bin_types::nil> {
  public:
    virtual ~_make_BinsVisitorBase() { }
    void visit() { } // YYY: because the 'using' statement (to silence warnings) needs it
  };

  typedef _make_BinsVisitorBase<bin_types::bins> VisitorBins;
  typedef _make_BinsVisitorBase<bin_types::bins_all> VisitorBinsAll;

  /**
   * Base class for the visitor pattern over every `Val`.
   */
  class Visitor : public VisitorLits, public VisitorBinsAll {
  public:
    void operator()(Val const& val);
  };

  class VisRepr : public Visitor {
  public:
    struct ReprCx {
      unsigned indents;
      bool top_level;
      bool single_line;
    };
    VisRepr(std::ostream& res, ReprCx cx={.top_level=true})
      : res(res)
      , cx(cx)
    { }

  private:
    std::ostream& res;
    struct ReprField {
      char const* name;
      enum {
        DBL,
        STR,
        VAL,
      } const data_ty;
      union {
        Val const* val;
        std::string const* str;
        double const dbl;
      } const data;
    };
    ReprCx cx;

    void reprHelper(Type const& type, char const* name, std::vector<ReprField> const fields);

    template <typename T>
    void visitCommon(T const&, std::false_type is_head);
    template <typename T>
    void visitCommon(T const&, std::true_type is_head);

  public:
    void visitNumLiteral(Type const& type, double n) override;
    void visitStrLiteral(Type const& type, std::string const& s) override;
    void visitLstLiteral(Type const& type, std::vector<Val*> const& v) override;
    void visitFunChain(Type const& type, std::vector<Fun*> const& f) override;
    void visitInput(Type const& type) override;
    void visitOutput(Type const& type) override;

    // XXX: would love any form of solution to this, but there might no be any du to the nature of the problem
    void visit(bins::abs_ const&) override;
    void visit(bins::abs_::Base const&) override;
    void visit(bins::add_ const&) override;
    void visit(bins::add_::Base const&) override;
    void visit(bins::add_::Base::Base const&) override;
    void visit(bins::flip_ const&) override;
    void visit(bins::flip_::Base const&) override;
    void visit(bins::flip_::Base::Base const&) override;
    void visit(bins::join_ const&) override;
    void visit(bins::join_::Base const&) override;
    void visit(bins::join_::Base::Base const&) override;
    void visit(bins::map_ const&) override;
    void visit(bins::map_::Base const&) override;
    void visit(bins::map_::Base::Base const&) override;
    void visit(bins::repeat_ const&) override;
    void visit(bins::repeat_::Base const&) override;
    void visit(bins::split_ const&) override;
    void visit(bins::split_::Base const&) override;
    void visit(bins::split_::Base::Base const&) override;
    void visit(bins::sub_ const&) override;
    void visit(bins::sub_::Base const&) override;
    void visit(bins::sub_::Base::Base const&) override;
    void visit(bins::tonum_ const&) override;
    void visit(bins::tonum_::Base const&) override;
    void visit(bins::tostr_ const&) override;
    void visit(bins::tostr_::Base const&) override;
    void visit(bins::zipwith_ const&) override;
    void visit(bins::zipwith_::Base const&) override;
    void visit(bins::zipwith_::Base::Base const&) override;
    void visit(bins::zipwith_::Base::Base::Base const&) override;
  };

} // namespace sel

#endif // SEL_VISITORS_HPP
