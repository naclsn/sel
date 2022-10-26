#include <iostream>
#include <sstream>

#include "sel/errors.hpp"
#include "sel/parser.hpp"

using namespace std;
using namespace sel;

void usage(char const* prog) {
  cerr << "Usage: " << prog << " [-Dch] <script...>\n";
  exit(EXIT_FAILURE);
}

void build(App& app, char const* const srcs[]) {
  stringstream source;
  while (*srcs)
    source << *srcs++ << ' ';
  source >> app;
}

int main(int argc, char const* const argv[]) {
  char const* prog = *argv++;
  if (argc < 2) usage(prog);

  // ...
  bool cla_debug = false;
  bool cla_typecheck = false;
  while (*argv && '-' == (*argv)[0]) {
    char c = (*argv)[1];
    switch (c) {
      case 'D': cla_debug = true;     argv++; break;
      case 'c': cla_typecheck = true; argv++; break;
      default:
        if (c < '0' || '9' < c) usage(prog);
        goto after_while;
    }
  }
  if (!*argv) usage(prog);
after_while: ;

  App app;
  build(app, argv);

  if (cla_debug) {
    app.repr(cout);
    return EXIT_SUCCESS;
  }

  if (cla_typecheck) {
    throw NIYError("type checking of user script", "- what -");
    return argc & 1;
  }

  app.runToEnd(cin, cout);

  return EXIT_SUCCESS;
}
