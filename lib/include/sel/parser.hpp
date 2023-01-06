#ifndef SEL_PARSER_HPP
#define SEL_PARSER_HPP

/**
 * Parsing of the main language and construction of an
 * Application from a user script.
 */

#include <ostream>
#include <istream>
#include <string>
#include <vector>
#include <unordered_map>

#include "engine.hpp"
#include "builtins.hpp"
#include "visitors.hpp"

namespace sel {

  class NumLiteral : public Num {
    double const n;
  public:
    NumLiteral(double n)
      : Num()
      , n(n)
    { }
    double value() override;
    void accept(Visitor& v) const override;
  };

  class StrLiteral : public Str {
    std::string const s;
    bool read;
  public:
    StrLiteral(std::string s)
      : Str(TyFlag::IS_FIN)
      , s(s)
      , read(false)
    { }
    std::ostream& stream(std::ostream& out) override;
    bool end() const override;
    std::ostream& entire(std::ostream& out) override;
    void accept(Visitor& v) const override;
  };

  class LstLiteral : public Lst {
    std::vector<Val*> const v;
    size_t c;
  public:
    LstLiteral(std::vector<Val*> v)
      : Lst(Type(Ty::LST,
          {.box_has=
            new std::vector<Type*>() // ZZZ: right...
          }, TyFlag::IS_FIN
        ))
      , v(v)
      , c(0)
    { }
    LstLiteral(std::vector<Val*> v, std::vector<Type*>* has) // ZZZ
      : Lst(Type(Ty::LST,
          {.box_has=
            has
          }, TyFlag::IS_FIN
        ))
      , v(v)
      , c(0)
    { }
    Val* operator*() override;
    Lst& operator++() override;
    bool end() const override;
    void accept(Visitor& v) const override;
  };

  // TODO: not this way, 'cause then it cannot be other
  // than `Fun` (eg. "repeat:1:, join::" should be `Str`)
  class FunChain : public Fun {
    std::vector<Fun*> const f;
  public:
    FunChain(std::vector<Fun*> f)
      : Fun(Type(Ty::FUN,
          {.box_pair={
            new Type(f[0]->type().from()),
            new Type(f[f.size() ? f.size()-1 : 0]->type().to()) // YYY: size-1
          }}, 0
        ))
      , f(f)
    { }
    Val* operator()(Val* arg) override;
    void accept(Visitor& v) const override;
  };

  class Input : public Str {
    std::istream& in;
    std::ostringstream cache;
    std::streamsize nowat = 0, upto = 0;
  public:
    Input(std::istream& in)
      : Str(TyFlag::IS_INF)
      , in(in)
    { }
    std::ostream& stream(std::ostream& out) override;
    bool end() const override;
    std::ostream& entire(std::ostream& out) override;
    void accept(Visitor& v) const override;
  };

  /**
   * An application is constructed from parsing a user
   * script. It serializes back to an equivalent script
   * (although it may not be strictly equal).
   */
  class App {
  private:
    Fun* f; // note that this does not have to be `Str -> Str`
    std::unordered_map<std::string, Val*> user;

  public:
    App() { }

    Val* lookup_name_user(std::string const& name);
    void define_name_user(std::string const& name, Val* v);

    void run(std::istream& in, std::ostream& out);

    // note: it always assumes `top_level`, even if false (ie. no single line)
    void repr(std::ostream& out, VisRepr::ReprCx cx={.top_level=true}) const;

    friend std::ostream& operator<<(std::ostream& out, App const& ty);
    friend std::istream& operator>>(std::istream& in, App& tt);
  };

} // namespace sel

#endif // SEL_PARSER_HPP
