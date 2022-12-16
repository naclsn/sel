#include "common.hpp"

#define showv_test(__ident, __expct) do {  \
  ostringstream oss;                       \
  VisRepr repr = VisRepr(oss);             \
  repr(*__ident);                          \
  cout << #__ident ": " << oss.str();      \
  assert_cmp(__expct, oss.str());          \
} while (0)

void test_tonum() { // tonum :3:
  Val* tonum1 = lookup_name("tonum");
  showv_test(tonum1,
    "<Str -> Num> Tonum1 { }\n"
    );

  Val* tonum0 = (*(Fun*)tonum1)(new StrLiteral("3"));
  showv_test(tonum0,
    "<Num> Tonum0 {\n"
    "   base=<Str -> Num> Tonum1 { }\n"
    "   arg=<Str> StrLiteral { s= \"3\" }\n"
    "}\n"
    );
}

void test_add() { // add 1 2
  Val* add2 = lookup_name("add");
  showv_test(add2,
    "<Num -> Num -> Num> Add2 { }\n"
    );

  Val* add1 = (*(Fun*)add2)(new NumLiteral(1));
  showv_test(add1,
    "<Num -> Num> Add1 {\n"
    "   base=<Num -> Num -> Num> Add2 { }\n"
    "   arg=<Num> NumLiteral { n= 1 }\n"
    "}\n"
    );

  Val* add0 = (*(Fun*)add1)(new NumLiteral(2));
  showv_test(add0,
    "<Num> Add0 {\n"
    "   base=<Num -> Num> Add1 {\n"
    "      base=<Num -> Num -> Num> Add2 { }\n"
    "      arg=<Num> NumLiteral { n= 1 }\n"
    "   }\n"
    "   arg=<Num> NumLiteral { n= 2 }\n"
    "}\n"
    );
}

void test_map() { // map tonum 4
  Val* map2 = lookup_name("map");
  showv_test(map2,
    "<(a -> b) -> [a] -> [b]> Map2 { }\n"
    );

  Val* map1 = (*(Fun*)map2)(lookup_name("tonum"));
  showv_test(map1,
    "<[Str]* -> [Num]*> Map1 {\n"
    "   base=<(a -> b) -> [a] -> [b]> Map2 { }\n"
    "   arg=<Str -> Num> Tonum1 { }\n"
    "}\n"
    );

  Val* map0 = (*(Fun*)map1)(new LstLiteral({new StrLiteral("4")}, new vector<Type*>({new Type(Ty::STR, {0}, 0)})));
  showv_test(map0,
    "<[Num]> Map0 {\n"
    "   base=<[Str]* -> [Num]*> Map1 {\n"
    "      base=<(a -> b) -> [a] -> [b]> Map2 { }\n"
    "      arg=<Str -> Num> Tonum1 { }\n"
    "   }\n"
    "   arg=<[Str]> LstLiteral { v[0]=<Str> StrLiteral { s= \"4\" } }\n"
    "}\n"
    );
}

void test_repeat() { // repeat 5
  Val* repeat1 = lookup_name("repeat");
  showv_test(repeat1,
    "<a -> [a]> Repeat1 { }\n"
    );

  Val* repeat0 = (*(Fun*)repeat1)(new NumLiteral(5));
  showv_test(repeat0,
    "<[Num]*> Repeat0 {\n"
    "   base=<a -> [a]> Repeat1 { }\n"
    "   arg=<Num> NumLiteral { n= 5 }\n"
    "}\n"
    );
}

void test_zipwith() { // zipwith map {repeat} {{1}} // YYY: but most boundedness are wrong
  Val* zipwith3 = lookup_name("zipwith");
  showv_test(zipwith3,
    "<(a -> b -> c) -> [a] -> [b] -> [c]> Zipwith3 { }\n"
    );

  Val* zipwith2 = (*(Fun*)zipwith3)(lookup_name("map"));
  showv_test(zipwith2,
    "<[a -> b]* -> [[a]]* -> [[b]]*> zipwith2 {\n"
    "   base=<(a -> b -> c) -> [a] -> [b] -> [c]> zipwith3 { }\n"
    "   arg=<(a -> b) -> [a] -> [b]> map2 { }\n"
    "}\n"
    );

  Val* _repeat1 = lookup_name("repeat");
  Val* zipwith1 = (*(Fun*)zipwith2)(new LstLiteral({_repeat1}, new vector<Type*>({new Type(_repeat1->type())})));
  showv_test(zipwith1,
    "<[[a]] -> [[[a]]]> zipwith1 {\n"
    "   base=<[a -> b]* -> [[a]]* -> [[b]]*> zipwith2 {\n"
    "      base=<(a -> b -> c) -> [a] -> [b] -> [c]> zipwith3 { }\n"
    "      arg=<(a -> b) -> [a] -> [b]> map2 { }\n"
    "   }\n"
    "   arg=<[a -> [a]]> LstLiteral { v[0]=<a -> [a]> repeat1 { } }\n"
    "}\n"
    );

  Val* _lst_lst_num = new LstLiteral({
    new LstLiteral({new NumLiteral(42)}, new vector<Type*>({new Type(Ty::NUM, {0}, 0)}))
  }, new vector<Type*>({new Type(Ty::LST, {.box_has=new vector<Type*>({new Type(Ty::NUM, {0}, 0)})}, TyFlag::IS_FIN)}));
  Val* zipwith0 = (*(Fun*)zipwith1)(_lst_lst_num);
  showv_test(zipwith0,
    "<[[[Num]*]*]*> zipwith0 {\n"
    "   base=<[[a]] -> [[[a]]]> zipwith1 {\n"
    "      base=<[a -> b]* -> [[a]]* -> [[b]]*> zipwith2 {\n"
    "         base=<(a -> b -> c) -> [a] -> [b] -> [c]> zipwith3 { }\n"
    "         arg=<(a -> b) -> [a] -> [b]> map2 { }\n"
    "      }\n"
    "      arg=<[a -> [a]]> LstLiteral { v[0]=<a -> [a]> repeat1 { } }\n"
    "   }\n"
    "   arg=<[[Num]]> LstLiteral { v[0]=<[Num]> LstLiteral { v[0]=<Num> NumLiteral { n= 42 } } }\n"
    "}\n"
    );
}

TEST(bins_applied) {
  cout << "{{{ test_tonum\n";   test_tonum();   cout << "}}}\n";
  cout << "{{{ test_add\n";     test_add();     cout << "}}}\n";
  cout << "{{{ test_map\n";     test_map();     cout << "}}}\n";
  cout << "{{{ test_repeat\n";  test_repeat();  cout << "}}}\n";
  cout << "{{{ test_zipwith\n"; test_zipwith(); cout << "}}}\n";
}
