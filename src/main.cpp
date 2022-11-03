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

int main0(int argc, char const* const argv[]) {
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

  app.run(cin, cout);

  return EXIT_SUCCESS;
}

static VisRepr repr = VisRepr(cerr);
#define showv(__ident) cerr << #__ident ": "; repr(*__ident)

void test_tonum() {
  Val* tonum1 = lookup_name("tonum");
  showv(tonum1);

  Val* tonum0 = (*(Fun*)tonum1)(new StrLiteral("3"));
  showv(tonum0);

  cerr << "result: " << (*(Num*)tonum0).value() << endl;
}

void test_add() {
  Val* add2 = lookup_name("add");
  showv(add2);

  Val* add1 = (*(Fun*)add2)(new NumLiteral(1));
  showv(add1);

  Val* add0 = (*(Fun*)add1)(new NumLiteral(2));
  showv(add0);

  cerr << "result: " << (*(Num*)add0).value() << endl;
}

void test_map() {
  Val* map2 = lookup_name("map");
  showv(map2);

  Val* map1 = (*(Fun*)map2)(lookup_name("tonum"));
  showv(map1);

  Val* map0 = (*(Fun*)map1)(new LstLiteral({new StrLiteral("4")}, types1(new Type(Ty::STR, {0}, 0))));
  showv(map0);

  cerr << "result: " << (*(Num*)*(*(Lst*)map0)).value() << endl;
}

int main() {
  cerr << "{{{ test_tonum\n"; test_tonum(); cerr << "}}}\n";
  cerr << "{{{ test_add\n";   test_add();   cerr << "}}}\n";
  cerr << "{{{ test_map\n";   test_map();   cerr << "}}}\n";
  return 0;
}
