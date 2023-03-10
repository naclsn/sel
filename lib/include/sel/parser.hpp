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

#include "engine.hpp"

namespace sel {

  struct NumLiteral : Num {
  private:
    double const n;

  public:
    NumLiteral(ref<Num> at, double n)
      : Num(at)
      , n(n)
    { }
    double value() override;
    ref<Val> copy() const override;

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
    StrLiteral(ref<Str> at, std::string s)
      : Str(at, TyFlag::IS_FIN)
      , s(s)
      , read(false)
    { }
    std::ostream& stream(std::ostream& out) override;
    bool end() override;
    std::ostream& entire(std::ostream& out) override;
    ref<Val> copy() const override;

    std::string const& underlying() const { return s; }

  protected:
    VisitTable visit_table() const override {
      return make_visit_table<decltype(this)>::function();
    }
  };

  struct LstLiteral : Lst {
  private:
    std::vector<ref<Val>> const v;
    size_t c;

  public:
    LstLiteral(ref<Lst> at, std::vector<ref<Val>> v)
      : Lst(at, Type::makeLst({/* ZZZ: right... */}, false, false))
      , v(v)
      , c(0)
    { }
    LstLiteral(ref<Lst> at, std::vector<ref<Val>> v, std::vector<Type>&& has) // ZZZ
      : Lst(at, Type::makeLst(std::move(has), false, false))
      , v(v)
      , c(0)
    { }
    ref<Val> operator*() override;
    Lst& operator++() override;
    bool end() override;
    ref<Val> copy() const override;

    std::vector<ref<Val>> const& underlying() const { return v; }

  protected:
    VisitTable visit_table() const override {
      return make_visit_table<decltype(this)>::function();
    }
  };

  struct FunChain : Fun {
  private:
    std::vector<ref<Fun>> f;

  public:
    FunChain(ref<Fun> at, std::vector<ref<Fun>> f)
      : Fun(at, Type::makeFun(
          Type(f[0]->type().from()),
          Type(f[f.size() ? f.size()-1 : 0]->type().to()) // YYY: size-1: a FunChain is never <1 from parsing
        ))
      , f(f)
    { }
    ref<Val> operator()(ref<Val> arg) override;
    ref<Val> copy() const override;

    std::vector<ref<Fun>> const& underlying() const { return f; }

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
    ref<U> v;

    Def(ref<U> at, std::string const name, std::string const doc, ref<U> v, Args... args)
      : U(at, std::forward<Args>(args)...)
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
    NumDefine(ref<Num> at, std::string const name, std::string const doc, ref<Num> v)
      : Def(at, name, doc, v)
    { }
    double value() override;
    ref<Val> copy() const override;

  protected:
    VisitTable visit_table() const override {
      return make_visit_table<decltype(this)>::function();
    }
  };

  struct StrDefine : Def<Str, bool> {
  public:
    StrDefine(ref<Str> at, std::string const name, std::string const doc, ref<Str> v)
      : Def(at, name, doc, v, v->type().is_inf())
    { }
    std::ostream& stream(std::ostream& out) override;
    bool end() override;
    std::ostream& entire(std::ostream& out) override;
    ref<Val> copy() const override;

  protected:
    VisitTable visit_table() const override {
      return make_visit_table<decltype(this)>::function();
    }
  };

  struct LstDefine : Def<Lst, Type&&> {
  public:
    LstDefine(ref<Lst> at, std::string const name, std::string const doc, ref<Lst> v)
      : Def(at, name, doc, v, Type(v->type()))
    { }
    ref<Val> operator*() override;
    Lst& operator++() override;
    bool end() override;
    ref<Val> copy() const override;

  protected:
    VisitTable visit_table() const override {
      return make_visit_table<decltype(this)>::function();
    }
  };

  struct FunDefine : Def<Fun, Type&&> {
  public:
    FunDefine(ref<Fun> at, std::string const name, std::string const doc, ref<Fun> v)
      : Def(at, name, doc, v, Type(v->type()))
    { }
    ref<Val> operator()(ref<Val> arg) override;
    ref<Val> copy() const override;

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

  public:
    // only to be used in an `Input`'s clone (not private for now so i can use `ref<Input>`)
    Input(ref<Str> at, Input const& other)
      : Str(at, TyFlag::IS_INF)
      , buffer(other.buffer)
      , first(false)
    { }

    Input(ref<Str> at, std::istream& in)
      : Str(at, TyFlag::IS_INF)
      , buffer(new InputBuffer(in))
      , first(true)
    { }
    ~Input() { if (first) delete buffer; }

    std::ostream& stream(std::ostream& out) override;
    bool end() override;
    std::ostream& entire(std::ostream& out) override;
    ref<Val> copy() const override;

    protected:
    VisitTable visit_table() const override {
      return make_visit_table<decltype(this)>::function();
    }
  };

  /// parses `in`, using `app` as the registery for values;
  /// not meant to be used outside of the application TU
  ref<Val> parseApplication(App& app, std::istream& in);

} // namespace sel

#endif // SEL_PARSER_HPP
