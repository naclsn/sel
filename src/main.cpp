#include <iostream>
#include <sstream>

#include "sel/parser.hpp"

using namespace std;
using namespace sel;

char const* prog;

int main(int argc, char const* const argv[]) {
  prog = *argv++;
  if (argc < 2) {
    cerr << "Usage: " << prog << " <script...>\n";
    return 1;
  }

  stringstream source;
  while (*argv)
    source << *argv++ << ' ';

  App app;
  source >> app;

  app.runToEnd(cin, cout);

  return 0;
}
