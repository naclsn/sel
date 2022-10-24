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

#include "engine.hpp"
#include "prelude.hpp"

namespace sel {

  class NumLiteral : public Num {
    double const n;
  public:
    NumLiteral(Env& env, double n)
      : Num(env)
      , n(n)
    { }
    double value() override;
  };

  class StrLiteral : public Str {
    std::string const s;
  public:
    StrLiteral(Env& env, std::string s)
      : Str(env, TyFlag::IS_FIN)
      , s(s)
    { }
    std::ostream& stream(std::ostream& out) override;
    void rewind() override;
  };

  class LstLiteral : public Lst {
    std::vector<Val*> const v;
    size_t c;
  public:
    LstLiteral(Env& env, std::vector<Val*> v)
      : Lst(env, Type(Ty::LST,
          {.box_has=
            new std::vector<Type*>() // ZZZ: right...
          }, TyFlag::IS_FIN
        ))
      , v(v)
      , c(0)
    { }
    Val* operator*() override;
    Lst& operator++() override;
    bool end() const override;
    void rewind() override;
    size_t count() override;
  };

  class FunChain : public Fun {
    std::vector<Fun*> const f;
  public:
    FunChain(Env& env, std::vector<Fun*> f)
      : Fun(env, Type(Ty::FUN,
          {.box_pair={
              new Type(f[0]->type().from()),
              new Type(f[f.size()-1]->type().to())
          }}, 0
        ))
      , f(f)
    { }
    Val* operator()(Val* arg) override;
  };

  // class StrStdin : public Str {
  //   StrStdin(Env& env)
  //     : Str(env, TyFlag::IS_INF)
  //   { }
  // };

  /**
   * An application is constructed from parsing a user
   * script. It serializes back to an equivalent script
   * (although it may not be strictly equal). The default
   * constructor automatically creates a new environment.
   */
  class App {
  private:
    Env env;
    std::vector<Fun*> funcs;

  public:
    App()
      : env(*this)
      , funcs()
    { }

    friend std::ostream& operator<<(std::ostream& out, App const& ty);
    friend std::istream& operator>>(std::istream& in, App& tt);
  };

} // namespace sel

#endif // SEL_PARSER_HPP
