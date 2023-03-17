#include "common.hpp"

#define showv_test(__ident, __expct) do {  \
  ostringstream oss;                       \
  VisRepr repr(oss);                       \
  __ident->accept(repr);                   \
  cout << #__ident ": " << oss.str();      \
  assert_cmp(__expct, oss.str());          \
} while (0)

App app;

int test_tonum() { // tonum :3:
  auto tonum1 = lookup_name(app, "tonum");
  showv_test(tonum1,
    "<Str* -> Num> Tonum1 { }\n"
    );

  auto tonum0 = (*(sel::ref<Fun>)tonum1)(sel::ref<StrLiteral>(app, "3"));
  showv_test(tonum0,
    "<Num> Tonum0 {\n"
    "   base=<Str* -> Num> Tonum1 { }\n"
    "   arg=<Str> StrLiteral { s= \"3\" }\n"
    "}\n"
    );

  return 0;
}

int test_add() { // add 1 2
  auto add2 = lookup_name(app, "add");
  showv_test(add2,
    "<Num -> Num -> Num> Add2 { }\n"
    );

  auto add1 = (*(sel::ref<Fun>)add2)(sel::ref<NumLiteral>(app, 1));
  showv_test(add1,
    "<Num -> Num> Add1 {\n"
    "   base=<Num -> Num -> Num> Add2 { }\n"
    "   arg=<Num> NumLiteral { n= 1 }\n"
    "}\n"
    );

  auto add0 = (*(sel::ref<Fun>)add1)(sel::ref<NumLiteral>(app, 2));
  showv_test(add0,
    "<Num> Add0 {\n"
    "   base=<Num -> Num> Add1 {\n"
    "      base=<Num -> Num -> Num> Add2 { }\n"
    "      arg=<Num> NumLiteral { n= 1 }\n"
    "   }\n"
    "   arg=<Num> NumLiteral { n= 2 }\n"
    "}\n"
    );

  return 0;
}

int test_map() { // map tonum 4
  auto map2 = lookup_name(app, "map");
  showv_test(map2,
    "<(a -> b) -> [a]* -> [b]*> Map2 { }\n"
    );

  auto map1 = (*(sel::ref<Fun>)map2)(lookup_name(app, "tonum"));
  showv_test(map1,
    "<[Str*]* -> [Num]*> Map1 {\n"
    "   base=<(a -> b) -> [a]* -> [b]*> Map2 { }\n"
    "   arg=<Str* -> Num> Tonum1 { }\n"
    "}\n"
    );

  auto map0 = (*(sel::ref<Fun>)map1)(sel::ref<LstLiteral>(app, Vals{sel::ref<StrLiteral>(app, "4")}, Types{Type::makeStr(false)}));
  showv_test(map0,
    "<[Num]> Map0 {\n"
    "   base=<[Str*]* -> [Num]*> Map1 {\n"
    "      base=<(a -> b) -> [a]* -> [b]*> Map2 { }\n"
    "      arg=<Str* -> Num> Tonum1 { }\n"
    "   }\n"
    "   arg=<[Str]> LstLiteral { v[0]=<Str> StrLiteral { s= \"4\" } }\n"
    "}\n"
    );

  return 0;
}

int test_repeat() { // repeat 5
  auto repeat1 = lookup_name(app, "repeat");
  showv_test(repeat1,
    "<a -> [a]*> Repeat1 { }\n"
    );

  auto repeat0 = (*(sel::ref<Fun>)repeat1)(sel::ref<NumLiteral>(app, 5));
  showv_test(repeat0,
    "<[Num]*> Repeat0 {\n"
    "   base=<a -> [a]*> Repeat1 { }\n"
    "   arg=<Num> NumLiteral { n= 5 }\n"
    "}\n"
    );

  return 0;
}

int test_zipwith() { // zipwith map {repeat} {{1}}
  auto zipwith3 = lookup_name(app, "zipwith");
  showv_test(zipwith3,
    "<(a -> b -> c) -> [a]* -> [b]* -> [c]*> Zipwith3 { }\n"
    );

  auto zipwith2 = (*(sel::ref<Fun>)zipwith3)(lookup_name(app, "map"));
  showv_test(zipwith2,
    "<[a -> b]* -> [[a]*]* -> [[b]*]*> Zipwith2 {\n"
    "   base=<(a -> b -> c) -> [a]* -> [b]* -> [c]*> Zipwith3 { }\n"
    "   arg=<(a -> b) -> [a]* -> [b]*> Map2 { }\n"
    "}\n"
    );

  auto _repeat1 = lookup_name(app, "repeat");
  auto zipwith1 = (*(sel::ref<Fun>)zipwith2)(sel::ref<LstLiteral>(app, Vals{_repeat1}, Types{Type(_repeat1->type())}));
  showv_test(zipwith1,
    // "<[[a]*] -> [[[a]*]*]> Zipwith1 {\n"
    "<[[a]] -> [[[a]*]]> Zipwith1 {\n"
    "   base=<[a -> b]* -> [[a]*]* -> [[b]*]*> Zipwith2 {\n"
    "      base=<(a -> b -> c) -> [a]* -> [b]* -> [c]*> Zipwith3 { }\n"
    "      arg=<(a -> b) -> [a]* -> [b]*> Map2 { }\n"
    "   }\n"
    "   arg=<[a -> [a]*]> LstLiteral { v[0]=<a -> [a]*> Repeat1 { } }\n"
    "}\n"
    );

  auto _lst_lst_num = sel::ref<LstLiteral>(app, Vals{
    sel::ref<LstLiteral>(app, Vals{sel::ref<NumLiteral>(app, 42)}, Types{Type::makeNum()})
  }, Types{Type::makeLst({Type::makeNum()}, false, false)});
  auto zipwith0 = (*(sel::ref<Fun>)zipwith1)(_lst_lst_num);
  showv_test(zipwith0,
    // "<[[[Num]]*]> Zipwith0 {\n"
    "<[[[Num]*]*]*> Zipwith0 {\n"
    // "   base=<[[a]*] -> [[[a]*]*]> Zipwith1 {\n"
    "   base=<[[a]] -> [[[a]*]]> Zipwith1 {\n"
    "      base=<[a -> b]* -> [[a]*]* -> [[b]*]*> Zipwith2 {\n"
    "         base=<(a -> b -> c) -> [a]* -> [b]* -> [c]*> Zipwith3 { }\n"
    "         arg=<(a -> b) -> [a]* -> [b]*> Map2 { }\n"
    "      }\n"
    "      arg=<[a -> [a]*]> LstLiteral { v[0]=<a -> [a]*> Repeat1 { } }\n"
    "   }\n"
    "   arg=<[[Num]]> LstLiteral { v[0]=<[Num]> LstLiteral { v[0]=<Num> NumLiteral { n= 42 } } }\n"
    "}\n"
    );

  return 0;
}

TEST(bins_applied) {
  int fails = 0;
  cout << "{{{ test_tonum\n";   fails+= test_tonum();   cout << "}}}\n";
  cout << "{{{ test_add\n";     fails+= test_add();     cout << "}}}\n";
  cout << "{{{ test_map\n";     fails+= test_map();     cout << "}}}\n";
  cout << "{{{ test_repeat\n";  fails+= test_repeat();  cout << "}}}\n";
  cout << "{{{ test_zipwith\n"; fails+= test_zipwith(); cout << "}}}\n";
  return fails;
}
