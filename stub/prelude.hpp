// place holder file
#ifndef SEL_PRELUDE_HPP
#define SEL_PRELUDE_HPP

#include "sel/utils.hpp"

#include <string>

#include "sel/engine.hpp"

namespace sel {

  // return may be nullptr
  Val* lookup_name(Environment& env, std::string const& name);
  Fun* lookup_unary(Environment& env, std::string const& name);
  Fun* lookup_binary(Environment& env, std::string const& name);

  struct Abs1 : Fun {
    Abs1()
      : Fun(numType(), numType())
    { }
    Val* operator()(Environment& env, Val* arg) override;
    void accept(Visitor& v) const override;
  };

  struct Abs0 : Num {
    Abs1* base;
    Num* arg;
    Abs0(Abs1* base, Num* arg)
      : base(base)
      , arg(arg)
    { }
    double value() override;
    void accept(Visitor& v) const override;
  };

  struct Add2 : Fun {
    Add2()
      : Fun(numType(), funType(new Type(numType()), new Type(numType())))
    { }
    Val* operator()(Environment& env, Val* arg) override;
    void accept(Visitor& v) const override;
  };

  struct Add1 : Fun {
    Add2* base;
    Num* arg;
    Add1(Add2* base, Num* arg)
      : Fun(numType(), numType())
      , base(base)
      , arg(arg)
    { }
    Val* operator()(Environment& env, Val* arg) override;
    void accept(Visitor& v) const override;
  };

  struct Add0 : Num {
    Add1* base;
    Num* arg;
    Add0(Add1* base, Num* arg)
      : base(base)
      , arg(arg)
    { }
    double value() override;
    void accept(Visitor& v) const override;
  };

}

#endif // SEL_PRELUDE_HPP
