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

#include "application.hpp"
#include "engine.hpp"

namespace sel {

  struct NumLiteral : Num {
  private:
    double const n;

  public:
    NumLiteral(double n): n(n) { }
    double value() override;
    std::unique_ptr<Val> copy() const override;

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
    StrLiteral(std::string&& s)
      : Str(TyFlag::IS_FIN)
      , s(move(s))
      , read(false)
    { }
    std::ostream& stream(std::ostream& out) override;
    bool end() override;
    std::ostream& entire(std::ostream& out) override;
    std::unique_ptr<Val> copy() const override;

    std::string const& underlying() const { return s; }

  protected:
    VisitTable visit_table() const override {
      return make_visit_table<decltype(this)>::function();
    }
  };

  struct LstLiteral : Lst {
  private:
    std::vector<std::unique_ptr<Val>> v;
    size_t c;

  public:
    LstLiteral(std::vector<std::unique_ptr<Val>>&& v)
      : Lst(Type::makeLst({/* ZZZ: right... */}, false, false))
      , v(move(v))
      , c(0)
    { }
    LstLiteral(std::vector<std::unique_ptr<Val>>&& v, std::vector<Type>&& has) // ZZZ
      : Lst(Type::makeLst(std::move(has), false, false))
      , v(move(v))
      , c(0)
    { }
    std::unique_ptr<Val> operator++() override;
    std::unique_ptr<Val> copy() const override;

    std::vector<std::unique_ptr<Val>> const& underlying() const { return v; }

  protected:
    VisitTable visit_table() const override {
      return make_visit_table<decltype(this)>::function();
    }
  };

  struct FunChain : Fun {
  private:
    std::vector<std::unique_ptr<Fun>> f;

  public:
    FunChain(std::vector<std::unique_ptr<Fun>>&& f)
      : Fun(Type::makeFun(
          Type(f[0]->type().from()),
          Type(f[f.size() ? f.size()-1 : 0]->type().to()) // YYY: size-1: a FunChain is never <1 from parsing
        ))
      , f(move(f))
    { }
    std::unique_ptr<Val> operator()(std::unique_ptr<Val> arg) override;
    std::unique_ptr<Val> copy() const override;

    std::vector<std::unique_ptr<Fun>> const& underlying() const { return f; }

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
    std::unique_ptr<U> v;

    Def(std::string const name, std::string const doc, std::unique_ptr<U>&& v, Args&&... args)
      : U(std::forward<Args>(args)...)
      , name(name)
      , doc(doc)
      , v(move(v))
    { }

  public:
    U const& underlying() const { return *v; }

    std::string const& getname() const { return name; }
    std::string const& getdoc() const { return doc; }
  };

  struct NumDefine : Def<Num> {
  public:
    NumDefine(std::string const name, std::string const doc, std::unique_ptr<Num> v)
      : Def(name, doc, move(v))
    { }
    double value() override;
    std::unique_ptr<Val> copy() const override;

  protected:
    VisitTable visit_table() const override {
      return make_visit_table<decltype(this)>::function();
    }
  };

  struct StrDefine : Def<Str, bool> {
  public:
    StrDefine(std::string const name, std::string const doc, std::unique_ptr<Str> v)
      : Def(name, doc, move(v), v->type().is_inf())
    { }
    std::ostream& stream(std::ostream& out) override;
    bool end() override;
    std::ostream& entire(std::ostream& out) override;
    std::unique_ptr<Val> copy() const override;

  protected:
    VisitTable visit_table() const override {
      return make_visit_table<decltype(this)>::function();
    }
  };

  struct LstDefine : Def<Lst, Type&&> {
  public:
    LstDefine(std::string const name, std::string const doc, std::unique_ptr<Lst> v)
      : Def(name, doc, move(v), Type(v->type()))
    { }
    std::unique_ptr<Val> operator++() override;
    std::unique_ptr<Val> copy() const override;

  protected:
    VisitTable visit_table() const override {
      return make_visit_table<decltype(this)>::function();
    }
  };

  struct FunDefine : Def<Fun, Type&&> {
  public:
    FunDefine(std::string const name, std::string const doc, std::unique_ptr<Fun> v)
      : Def(name, doc, move(v), Type(v->type()))
    { }
    std::unique_ptr<Val> operator()(std::unique_ptr<Val> arg) override;
    std::unique_ptr<Val> copy() const override;

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

    Input(Input const& other)
      : Str(TyFlag::IS_INF)
      , buffer(other.buffer)
      , first(false)
    { }
  public:

    Input(std::istream& in)
      : Str(TyFlag::IS_INF)
      , buffer(new InputBuffer(in))
      , first(true)
    { }
    ~Input() { if (first) delete buffer; }

    std::ostream& stream(std::ostream& out) override;
    bool end() override;
    std::ostream& entire(std::ostream& out) override;
    std::unique_ptr<Val> copy() const override;

    protected:
    VisitTable visit_table() const override {
      return make_visit_table<decltype(this)>::function();
    }
  };

  /// parses `in`, using `app` as the registery for values;
  /// not meant to be used outside of the application TU
  std::unique_ptr<Val> parseApplication(App& app, std::istream& in);

} // namespace sel

#endif // SEL_PARSER_HPP
