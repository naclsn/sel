#include <iostream>
#include <sstream>

#include "sel/visitors.hpp"
#include "sel/parser.hpp"

using namespace std;
using namespace sel;

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

  Application app;
  Environment noenv = app.environ();

  Fun* add2 = new Add2();
  show("add2", add2);

  Fun* add1 = (Fun*)add2->operator()(noenv, a);
  show("add1", add1);

  Num* add0 = (Num*)add1->operator()(noenv, b);
  show("add0", add0);

  double res = add0->value();
  cout << "res: " << res << endl;

  // delete a;
}

void parseApplication() {
  istringstream iss("split : :, map +1, join ::::");

  Application app;
  iss >> app;

  // ostringstream oss;
  // oss << app;
}

int main() {
  // bidoof();
  // numLiteral();
  // parseApplication();

  cerr << "=== before constructing\n";
  // auto a = Add2();
  // auto a = NumLiteral(43);
  auto a = Abs1();
  // auto a = Bidoof("A");
  cerr << "=== after constructing\n";

  return 0;
}
