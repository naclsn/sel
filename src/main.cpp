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
  char const* source = "split : :, map [tonum, add 1, add 2, tostr], join ::::";
  cout << "source: '" << source << "'\n";

  istringstream iss(source);
  App app;
  iss >> app;

  cout << "parsed app:\n";
  // cout << app << endl;
  app.repr(cout, {.top_level=true});

  cout << "\n====\n\n";

  istringstream input("1 2 3");
  ostringstream output;
  app.run(input, output);
  cout << output.str();
}

void doTheThing(int argc, char* argv[]) {
  stringstream source;
  for (int k = 1; k < argc; k++)
    source << argv[k] << ' ';

  App app;
  source >> app;

  app.runToEnd(cin, cout);
}

int main(int argc, char* argv[]) {
  // test_parseApp();
  doTheThing(argc, argv);

  return 0;
}
