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
#include "visitors.hpp"

namespace sel {

  class NumLiteral : public Num {
    double const n;
  public:
    NumLiteral(Env& env, double n)
      : Num(env)
      , n(n)
    { }
    double value() override;
    void accept(Visitor& v) const override;
  };

  class StrLiteral : public Str {
    std::string const s;
    bool read;
  public:
    StrLiteral(Env& env, std::string s)
      : Str(env, TyFlag::IS_FIN)
      , s(s)
      , read(false)
    { }
    std::ostream& stream(std::ostream& out) override;
    bool end() const override;
    void rewind() override;
    std::ostream& entire(std::ostream& out) override;
    void accept(Visitor& v) const override;
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
    void accept(Visitor& v) const override;
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
    void accept(Visitor& v) const override;
  };

  class Stdin : public Fun {
    std::istream* in;
  public:
    Stdin(Env& env)
      : Fun(env, Type(Ty::FUN,
          {.box_pair={
            new Type(Ty::UNK, {.name=new std::string("()")}, 0),
            new Type(Ty::STR, {0}, TyFlag::IS_FIN) // YYY: indeed, it reads a line, which is assumed to be of finite size
          }}, 0
        ))
      , in(nullptr)
    { }
    Val* operator()(Val* arg) override;
    void accept(Visitor& v) const override;
    void setIn(std::istream* in) { this->in = in; }
  };

  class Stdout : public Fun {
    std::ostream* out;
  public:
    Stdout(Env& env)
      : Fun(env, Type(Ty::FUN,
          {.box_pair={
            new Type(Ty::STR, {0}, TyFlag::IS_INF), // YYY: will consider output may be infinite
            new Type(Ty::UNK, {.name=new std::string("()")}, 0)
          }}, 0
        ))
      , out(nullptr)
    { }
    Val* operator()(Val* arg) override;
    void accept(Visitor& v) const override;
    void setOut(std::ostream* out) { this->out = out; }
  };

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
    Stdin* fin;
    Stdout* fout;

  public:
    App()
      : env(*this)
      , funcs()
    { }

    void run(std::istream& in, std::ostream& out);

    void repr(std::ostream& out, VisRepr::ReprCx cx={.top_level=true}) const;

    friend std::ostream& operator<<(std::ostream& out, App const& ty);
    friend std::istream& operator>>(std::istream& in, App& tt);
  };

} // namespace sel

#endif // SEL_PARSER_HPP
