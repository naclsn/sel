#include "common.hpp"

#define showv_test(__ident, __expct) do {  \
  ostringstream oss;                       \
  VisRepr repr(oss);                       \
  __ident->accept(repr);                   \
  cout << #__ident ": " << oss.str();      \
  assert_cmp(__expct, oss.str());          \
} while (0)

int test_tonum() { // tonum :3:
  auto tonum1 = lookup_name("tonum");
  showv_test(tonum1,
    "<Str* -> Num> Tonum1 { }\n"
    );

  auto tonum0 = (*val_cast<Fun>(move(tonum1)))(make_unique<StrLiteral>("3"));
  showv_test(tonum0,
    "<Num> Tonum0 { arg_A=<Str> StrLiteral { s= \"3\" } }\n"
    );

  return 0;
}

int test_add() { // add 1 2
  auto add2 = lookup_name("add");
  showv_test(add2,
    "<Num -> Num -> Num> Add2 { }\n"
    );

  auto add1 = (*val_cast<Fun>(move(add2)))(make_unique<NumLiteral>(1));
  showv_test(add1,
    "<Num -> Num> Add1 { arg_A=<Num> NumLiteral { n= 1 } }\n"
    );

  auto add0 = (*val_cast<Fun>(move(add1)))(make_unique<NumLiteral>(2));
  showv_test(add0,
    "<Num> Add0 {\n"
    "   arg_A=<Num> NumLiteral { n= 1 }\n"
    "   arg_B=<Num> NumLiteral { n= 2 }\n"
    "}\n"
    );

  return 0;
}

int test_map() { // map tonum 4
  auto map2 = lookup_name("map");
  showv_test(map2,
    "<(a -> b) -> [a]* -> [b]*> Map2 { }\n"
    );

  auto map1 = (*val_cast<Fun>(move(map2)))(lookup_name("tonum"));
  showv_test(map1,
    "<[Str*]* -> [Num]*> Map1 { arg_A=<Str* -> Num> Tonum1 { } }\n"
    );

  auto map0 = (*val_cast<Fun>(move(map1)))(make_unique<LstLiteral>(make_vals(make_unique<StrLiteral>("4")), Types{Type::makeStr(false)}));
  showv_test(map0,
    "<[Num]> Map0 {\n"
    "   arg_A=<Str* -> Num> Tonum1 { }\n"
    "   arg_B=<[Str]> LstLiteral { v[0]=<Str> StrLiteral { s= \"4\" } }\n"
    "}\n"
    );

  return 0;
}

int test_repeat() { // repeat 5
  auto repeat1 = lookup_name("repeat");
  showv_test(repeat1,
    "<a -> [a]*> Repeat1 { }\n"
    );

  auto repeat0 = (*val_cast<Fun>(move(repeat1)))(make_unique<NumLiteral>(5));
  showv_test(repeat0,
    "<[Num]*> Repeat0 { arg_A=<Num> NumLiteral { n= 5 } }\n"
    );

  return 0;
}

int test_zipwith() { // zipwith map {repeat} {{1}}
  auto zipwith3 = lookup_name("zipwith");
  showv_test(zipwith3,
    "<(a -> b -> c) -> [a]* -> [b]* -> [c]*> Zipwith3 { }\n"
    );

  auto zipwith2 = (*val_cast<Fun>(move(zipwith3)))(lookup_name("map"));
  showv_test(zipwith2,
    "<[a -> b]* -> [[a]*]* -> [[b]*]*> Zipwith2 { arg_A=<(a -> b) -> [a]* -> [b]*> Map2 { } }\n"
    );

  auto _repeat1 = lookup_name("repeat");
  auto zipwith1 = (*val_cast<Fun>(move(zipwith2)))(make_unique<LstLiteral>(make_vals(move(_repeat1)), Types{Type(_repeat1->type())}));
  showv_test(zipwith1,
    // "<[[a]*] -> [[[a]*]*]> Zipwith1 {\n"
    "<[[a]] -> [[[a]*]]> Zipwith1 {\n"
    "   arg_A=<(a -> b) -> [a]* -> [b]*> Map2 { }\n"
    "   arg_B=<[a -> [a]*]> LstLiteral { v[0]=<a -> [a]*> Repeat1 { } }\n"
    "}\n"
    );

  auto _lst_lst_num = make_unique<LstLiteral>(make_vals(
    make_unique<LstLiteral>(make_vals(make_unique<NumLiteral>(42)), Types{Type::makeNum()})
  ), Types{Type::makeLst({Type::makeNum()}, false, false)});
  auto zipwith0 = (*val_cast<Fun>(move(zipwith1)))(move(_lst_lst_num));
  showv_test(zipwith0,
    // "<[[[Num]]*]> Zipwith0 {\n"
    "<[[[Num]*]*]*> Zipwith0 {\n"
    "   arg_A=<(a -> b) -> [a]* -> [b]*> Map2 { }\n"
    "   arg_B=<[a -> [a]*]> LstLiteral { v[0]=<a -> [a]*> Repeat1 { } }\n"
    "   arg_C=<[[Num]]> LstLiteral { v[0]=<[Num]> LstLiteral { v[0]=<Num> NumLiteral { n= 42 } } }\n"
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
