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

#include "types.hpp"

#include "prelude_visit_each"

/// __do(__name, ...) .. __VA_ARGS__ ..
#define VISIT_EACH(__do) PRELUDE_VISIT_EACH(__do)  \
  __do(Bidoof, Type const& type)                   \
  __do(NumLiteral, Type const& type, double)       \

namespace sel {

  class Val;

  /**
   * Base class for the visitor pattern over every `Val`.
   */
  class Visitor {
  public:
    virtual ~Visitor() { }
    void operator()(Val const& val);

#define DO(__n, ...) virtual void visit##__n(__VA_ARGS__) = 0;
    VISIT_EACH(DO)
#undef DO
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
#define DO(__n, ...) void visit##__n(__VA_ARGS__) override;
    VISIT_EACH(DO)
#undef DO
  };

} // namespace sel

#endif // SEL_VISITORS_HPP
