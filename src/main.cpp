#include <iostream>
#include <sstream>

#include "sel/visitors.hpp"
#include "sel/parser.hpp"

using namespace std;
using namespace sel;

void test_parseApp() {
  char const* source = "split : :, map [add 1], join ::::";
  istringstream iss(source);
  App app;
  iss >> app;

  cout
    << "source: '" << source << "'\n"
    << "parsed app:\n" << app << endl
  ;
}

int main() {
  test_parseApp();

  return 0;
}
