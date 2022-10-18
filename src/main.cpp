#include <iostream>
#include <sstream>

#include "sel/visitors.hpp"
#include "sel/bidoof.hpp"
#include "sel/parser.hpp"

using namespace std;
using namespace sel;

void bidoof() {
  std::ostringstream oss;
  ValRepr repr = ValRepr(oss, {});
  Bidoof bidoof = Bidoof();

  repr(bidoof);

  cout << oss.str() << "\n";
}


template <typename To>
To* coerse(Val* from) { return (To*)from; }


struct Add2 : Fun {
  Add2()
    : Fun(numType(), funType(new Type(numType()), new Type(numType())))
  { }
  Val* operator()(Val* arg) override;
  void accept(Visitor& v) const override { v.visitAdd2(type()); }
};

struct Add1 : Fun {
  Add2* base;
  Num* arg;
  Add1(Add2* base, Num* arg)
    : Fun(numType(), numType())
    , base(base)
    , arg(arg)
  { }
  Val* operator()(Val* arg) override;
  void accept(Visitor& v) const override { v.visitAdd1(type(), base, arg); }
};

struct Add0 : Num {
  Add1* base;
  Num* arg;
  Add0(Add1* base, Num* arg)
    : base(base)
    , arg(arg)
  { }
  double value() override;
  void accept(Visitor& v) const override { v.visitAdd0(type(), base, arg); }
};


Val* Add2::operator()(Val* arg) {
  return new Add1(this, coerse<Num>(arg));
}
Val* Add1::operator()(Val* arg) {
  return new Add0(this, coerse<Num>(arg));
}
double Add0::value() {
  return base->arg->value() + arg->value();
}



void show(char const* name, Val* val) {
  static auto repr = ValRepr(cout, {});
  cout << name << " := ";
  repr(*val);
  cout << "\n";
}

void numLiteral() {
  Val* a = new NumLiteral(42);
  Val* b = new NumLiteral(12);
  show("a", a);
  show("b", b);

  Fun* add2 = new Add2();
  show("add2", add2);

  Fun* add1 = (Fun*)add2->operator()(a);
  show("add1", add1);

  Num* add0 = (Num*)add1->operator()(b);
  show("add0", add0);

  double res = add0->value();
  cout << "res: " << res << endl;

  // delete a;
}

int main() {
  // bidoof();
  numLiteral();
  return 0;
}
