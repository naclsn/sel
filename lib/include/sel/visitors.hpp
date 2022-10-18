#ifndef SEL_VISITORS_HPP
#define SEL_VISITORS_HPP

/**
 * Setup for "visitor pattern" over every derivatives of
 * `Val`. Also defines a few such as `ValRepr`. Note that
 * this pattern may be quite heavy and is not meant to be
 * implemented where performance matters.
 */

#include <ostream>
#include <string>

// #define VIS_LIST
// #define VIS_ADD(__ident) VIS_LIST; 
// #define VIS_ACCEPT(__ident) void accept(Visitor& v) { v.visit##__add_name(some); }

// #include "engine.hpp"
#include "types.hpp"

namespace sel {

  class Val;

  /**
   * Base class for the visitor pattern over every `Val`.
   */
  class Visitor {
  public:
    virtual ~Visitor() { }
    void operator()(Val const& val);

    virtual void visitBidoof(Type const& type, char const* some, Val const* other) = 0;
    virtual void visitNumLiteral(Type const& type, double n) = 0;
    virtual void visitAdd2(Type const& type) = 0;
    virtual void visitAdd1(Type const& type, Val const* base, Val const* arg) = 0;
    virtual void visitAdd0(Type const& type, Val const* base, Val const* arg) = 0;
  };

  class ValRepr : public Visitor {
  public:
    struct ReprCx {
      unsigned indents;
      bool single_line;
    };
    ValRepr(std::ostream& res, ReprCx cx)
      : res(res)
      , cx(cx)
    { }
  private:
    std::ostream& res;
    struct ReprField {
      char const* name;
      enum {
        CHR,
        STR,
        VAL,
      } const data_ty;
      union {
        Val const* val;
        std::string const* str;
        char const* chr;
      } const data;
    };
    ReprCx cx;
    void reprHelper(Type const& type, char const* name, std::initializer_list<ReprField const> const fields);

  public:
    void visitBidoof(Type const& type, char const* some, Val const* other) override;
    void visitNumLiteral(Type const& type, double n) override;
    void visitAdd2(Type const& type) override;
    void visitAdd1(Type const& type, Val const* base, Val const* arg) override;
    void visitAdd0(Type const& type, Val const* base, Val const* arg) override;
  };

} // namespace sel

#endif // SEL_VISITORS_HPP
