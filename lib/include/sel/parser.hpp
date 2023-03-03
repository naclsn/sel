#ifndef SEL_PARSER_HPP
#define SEL_PARSER_HPP

/**
 * Parsing of the main language and construction of an
 * Application from a user script.
 */

#include <istream>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "builtins.hpp"
#include "engine.hpp"
#include "visitors.hpp"

namespace sel {

  struct NumLiteral : Num {
  private:
    double const n;

  public:
    NumLiteral(App& app, double n)
      : Num(app)
      , n(n)
    { }
    double value() override;
    Val* copy() const override;

    double underlying() const { return n; }

  protected:
    VisitTable visit_table() const override {
      return make_visit_table<decltype(this)>::function();
    }
  };

  struct StrLiteral : Str {
  private:
    std::string const s;
    bool read;

  public:
    StrLiteral(App& app, std::string s)
      : Str(app, TyFlag::IS_FIN)
      , s(s)
      , read(false)
    { }
    std::ostream& stream(std::ostream& out) override;
    bool end() override;
    std::ostream& entire(std::ostream& out) override;
    Val* copy() const override;

    std::string const& underlying() const { return s; }

  protected:
    VisitTable visit_table() const override {
      return make_visit_table<decltype(this)>::function();
    }
  };

  struct LstLiteral : Lst {
  private:
    std::vector<Val*> const v;
    size_t c;

  public:
    LstLiteral(App& app, std::vector<Val*> v)
      : Lst(app, Type::makeLst({/* ZZZ: right... */}, false, false))
      , v(v)
      , c(0)
    { }
    LstLiteral(App& app, std::vector<Val*> v, std::vector<Type>&& has) // ZZZ
      : Lst(app, Type::makeLst(std::move(has), false, false))
      , v(v)
      , c(0)
    { }
    Val* operator*() override;
    Lst& operator++() override;
    bool end() override;
    Val* copy() const override;

    std::vector<Val*> const& underlying() const { return v; }

  protected:
    VisitTable visit_table() const override {
      return make_visit_table<decltype(this)>::function();
    }
  };

  struct FunChain : Fun {
  private:
    std::vector<Fun*> const f;

  public:
    FunChain(App& app, std::vector<Fun*> f)
      : Fun(app, Type::makeFun(
          Type(f[0]->type().from()),
          Type(f[f.size() ? f.size()-1 : 0]->type().to()) // YYY: size-1: a FunChain is never <1 from parsing
        ))
      , f(f)
    { }
    Val* operator()(Val* arg) override;
    Val* copy() const override;

    std::vector<Fun*> const& underlying() const { return f; }

  protected:
    VisitTable visit_table() const override {
      return make_visit_table<decltype(this)>::function();
    }
  };

  template <typename U, typename ...Args>
  struct Def : U {
  protected:
    std::string const name;
    std::string const doc;
    U* v;

    Def(App& app, std::string const name, std::string const doc, U* v, Args... args)
      : U(app, args...)
      , name(name)
      , doc(doc)
      , v(v)
    { }

  public:
    U const& underlying() const { return *v; }

    std::string const& getname() const { return name; }
    std::string const& getdoc() const { return doc; }
  };

  struct NumDefine : Def<Num> {
  public:
    NumDefine(App& app, std::string const name, std::string const doc, Num* v)
      : Def(app, name, doc, v)
    { }
    double value() override;
    Val* copy() const override;

  protected:
    VisitTable visit_table() const override {
      return make_visit_table<decltype(this)>::function();
    }
  };

  struct StrDefine : Def<Str, bool> {
  public:
    StrDefine(App& app, std::string const name, std::string const doc, Str* v)
      : Def(app, name, doc, v, v->type().is_inf())
    { }
    std::ostream& stream(std::ostream& out) override;
    bool end() override;
    std::ostream& entire(std::ostream& out) override;
    Val* copy() const override;

  protected:
    VisitTable visit_table() const override {
      return make_visit_table<decltype(this)>::function();
    }
  };

  struct LstDefine : Def<Lst, Type const&> {
  public:
    LstDefine(App& app, std::string const name, std::string const doc, Lst* v)
      : Def(app, name, doc, v, v->type())
    { }
    Val* operator*() override;
    Lst& operator++() override;
    bool end() override;
    Val* copy() const override;

  protected:
    VisitTable visit_table() const override {
      return make_visit_table<decltype(this)>::function();
    }
  };

  struct FunDefine : Def<Fun, Type const&> {
  public:
    FunDefine(App& app, std::string const name, std::string const doc, Fun* v)
      : Def(app, name, doc, v, v->type())
    { }
    Val* operator()(Val* arg) override;
    Val* copy() const override;

  protected:
    VisitTable visit_table() const override {
      return make_visit_table<decltype(this)>::function();
    }
  };

  struct Input : Str {
  private:
    struct InputBuffer {
      std::istream& in;
      std::ostringstream cache;
      std::streamsize upto = 0;
      InputBuffer(std::istream& in): in(in) { }
    }* buffer;
    std::streamsize nowat = 0;
    bool first;

    Input(App& app, Input const& other)
      : Str(app, TyFlag::IS_INF)
      , buffer(other.buffer)
      , first(false)
    { }

  public:
    Input(App& app, std::istream& in)
      : Str(app, TyFlag::IS_INF)
      , buffer(new InputBuffer(in))
      , first(true)
    { }
    ~Input() { if (first) delete buffer; }

    std::ostream& stream(std::ostream& out) override;
    bool end() override;
    std::ostream& entire(std::ostream& out) override;
    Val* copy() const override;

    protected:
    VisitTable visit_table() const override {
      return make_visit_table<decltype(this)>::function();
    }
  };

  /**
   * An application is constructed from parsing a user
   * script. It serializes back to an equivalent script
   * (although it may not be strictly equal).
   */
  class App {
  private:
    Val* f; // note that this does not have to be `Str -> Str`
    std::unordered_map<std::string, Val*> user;

    std::vector<Val const*> ptrs;

    bool strict_type = false;
    bool not_fun = false; // ie yes fun by default

  public:
    App() { }
    App(bool strict_type, bool not_fun)
      : strict_type(strict_type)
      , not_fun(not_fun)
    { }
    ~App() { clear(); }

    void push_back(Val const* v);
    void clear();

    Type const& type() const { return f->type(); }

    bool is_strict_type() const { return strict_type; }

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
