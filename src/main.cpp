#include <iostream>
#include <sstream>

#include "sel/visitors.hpp"
#include "sel/parser.hpp"

using namespace std;
using namespace sel;

void test_repr() {
  Env env = Env(App());
  Val* num = new NumLiteral(env, 42.1);
  VisRepr repr = VisRepr(cout, {});
  repr(*num);
}

void test_parseApp() {
  char const* source = "split : :, map [add 1, add 2], join ::::";
  cout << "source: '" << source << "'\n";

  istringstream iss(source);
  App app;
  iss >> app;

  cout << "parsed app:\n";
  // cout << app << endl;
  app.repr(cout, {.indents=1, .top_level=true});
}

int main() {
  test_parseApp();

  return 0;
}
