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
#include <vector>

#include "types.hpp"

namespace sel {

  class Val;
  class Fun; // YYY: for FunChain, but I would prefer without

  /**
   * Base class for the visitor pattern over every `Val`.
   */
  class Visitor {
  public:
    virtual ~Visitor() { }
    void operator()(Val const& val);

    virtual void visitNumLiteral(Type const& type, double n) = 0;
    virtual void visitStrLiteral(Type const& type, std::string const& s) = 0;
    virtual void visitLstLiteral(Type const& type, std::vector<Val*> const& v) = 0;
    virtual void visitFunChain(Type const& type, std::vector<Fun*> const& f) = 0;
    virtual void visitInput(Type const& type) = 0;
    virtual void visitOutput(Type const& type) = 0;
    virtual void visitHead(std::string some, Type const& type) = 0;
    virtual void visitBody(std::string some, Type const& type, Val const* base, Val const* arg) = 0;
    virtual void visitTail(std::string some, Type const& type, Val const* base, Val const* arg) = 0;
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

    void reprHelper(Type const& type, char const* name, std::initializer_list<ReprField> const fields);
    void reprHelper(Type const& type, char const* name, std::vector<ReprField> const fields);

  public:
    void visitNumLiteral(Type const& type, double n) override;
    void visitStrLiteral(Type const& type, std::string const& s) override;
    void visitLstLiteral(Type const& type, std::vector<Val*> const& v) override;
    void visitFunChain(Type const& type, std::vector<Fun*> const& f) override;
    void visitInput(Type const& type) override;
    void visitOutput(Type const& type) override;
    void visitHead(std::string some, Type const& type) override;
    void visitBody(std::string some, Type const& type, Val const* base, Val const* arg) override;
    void visitTail(std::string some, Type const& type, Val const* base, Val const* arg) override;
  };

} // namespace sel

#endif // SEL_VISITORS_HPP
