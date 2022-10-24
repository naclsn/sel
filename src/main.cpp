#include <iostream>
#include <sstream>

#include "sel/visitors.hpp"
#include "sel/parser.hpp"

using namespace std;
using namespace sel;

void test_parseApp() {
  char const* source = "split : :, map [add 1, add 2], join ::::";
  cout << "source: '" << source << "'\n";

  istringstream iss(source);
  App app;
  iss >> app;

  cout << "parsed app:\n" << app << endl;
}

int main() {
  test_parseApp();

  return 0;
}
