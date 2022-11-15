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

static VisRepr repr = VisRepr(cout);
#define showv(__ident) cout << #__ident ": "; repr(*__ident)

void test_tonum() {
  Val* tonum1 = lookup_name("tonum");
  showv(tonum1);

  Val* tonum0 = (*(Fun*)tonum1)(new StrLiteral("3"));
  showv(tonum0);

  cout << "result: " << (*(Num*)tonum0).value() << endl;
}

void test_add() {
  Val* add2 = lookup_name("add");
  showv(add2);

  Val* add1 = (*(Fun*)add2)(new NumLiteral(1));
  showv(add1);

  Val* add0 = (*(Fun*)add1)(new NumLiteral(2));
  showv(add0);

  cout << "result: " << (*(Num*)add0).value() << endl;
}

void test_map() {
  Val* map2 = lookup_name("map");
  showv(map2);

  Val* map1 = (*(Fun*)map2)(lookup_name("tonum"));
  showv(map1);

  Val* map0 = (*(Fun*)map1)(new LstLiteral({new StrLiteral("4")}, types1(new Type(Ty::STR, {0}, 0))));
  showv(map0);

  cout << "result: " << (*(Num*)*(*(Lst*)map0)).value() << endl;
}

void test_repeat() {
  Val* repeat1 = lookup_name("repeat");
  showv(repeat1);

  Val* repeat0 = (*(Fun*)repeat1)(new NumLiteral(5));
  showv(repeat0);

  // cout << "result: " << (*(Num*)*(*(Lst*)repeat0)).value() << endl;
}

void test_zipwith() { // zipwith map {repeat} {{1}}
  Val* zipwith3 = lookup_name("zipwith");
  showv(zipwith3);

  // Val* zipwith2 = (*(Fun*)zipwith3)(lookup_name("add"));
  Val* _map2 = lookup_name("map");
  showv(_map2);
  Val* zipwith2 = (*(Fun*)zipwith3)(_map2);
  showv(zipwith2);

  // Val* zipwith1 = (*(Fun*)zipwith2)(new LstLiteral({new NumLiteral(37)}, types1(new Type(Ty::NUM, {0}, 0))));
  Val* _repeat1 = lookup_name("repeat");
  showv(_repeat1);
  Val* zipwith1 = (*(Fun*)zipwith2)(new LstLiteral({_repeat1}, types1(new Type(_repeat1->type()))));
  showv(zipwith1);

  // Val* zipwith0 = (*(Fun*)zipwith1)(new LstLiteral({new NumLiteral(42)}, types1(new Type(Ty::NUM, {0}, 0))));
  Val* _lst_lst_num = new LstLiteral({
    new LstLiteral({new NumLiteral(42)}, types1(new Type(Ty::NUM, {0}, 0)))
  }, types1(new Type(Ty::LST, {.box_has=types1(new Type(Ty::NUM, {0}, 0))}, TyFlag::IS_FIN)));
  showv(_lst_lst_num);
  Val* zipwith0 = (*(Fun*)zipwith1)(_lst_lst_num);
  showv(zipwith0);
}

int main() {
  // cout << "{{{ test_tonum\n";   test_tonum();   cout << "}}}\n";
  // cout << "{{{ test_add\n";     test_add();     cout << "}}}\n";
  // cout << "{{{ test_map\n";     test_map();     cout << "}}}\n";
  // cout << "{{{ test_repeat\n";  test_repeat();  cout << "}}}\n";
  cout << "{{{ test_zipwith\n"; test_zipwith(); cout << "}}}\n";
  return 0;
}
