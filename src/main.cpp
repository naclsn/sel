#include <iostream>
#include <sstream>

#include "sel/visitors.hpp"
#include "sel/bidoof.hpp"
#include "sel/parser.hpp"

#include "prelude.hpp"

using namespace std;
using namespace sel;

void bidoof() {
  std::ostringstream oss;
  ValRepr repr = ValRepr(oss, {});
  Bidoof bidoof = Bidoof("A");

  repr(**bidoof);

  cout << oss.str() << "\n";
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
