#include "common.hpp"

void doTestEq(char const* source, char const* repr) {
  istringstream iss(source);
  App app;
  iss >> app;

  cout << "successfully parsed source '" << source << "'" << endl;

  ostringstream oss;
  app.repr(oss);

  cout << "application representation:\n" << oss.str() << endl;
  assert_cmp(repr, oss.str());
}

TEST(parseApp) {
  doTestEq(
    "split : :, map [tonum, add 1, add 2, tostr], join ::::"
    ,
    "<Str*> Input { }\n"
    "<Str* -> [Str*]*> Split1 {\n"
    // "<Str* -> [Str]*> Split1 {\n"
    "   base=<Str -> Str -> [Str]> Split2 { }\n"
    // "   base=<Str -> Str* -> [Str]*> Split2 { }\n"
    "   arg=<Str> StrLiteral { s= \" \" }\n"
    "}\n"
    "<[Str]* -> [Str]*> Map1 {\n"
    "   base=<(a -> b) -> [a] -> [b]> Map2 { }\n"
    // "   base=<(a -> b) -> [a]* -> [b]*> Map2 { }\n"
    "   arg=<Str -> Str> FunChain {\n"
    "      f[0]=<Str -> Num> Tonum1 { }\n"
    "      f[1]=<Num -> Num> Add1 {\n"
    "         base=<Num -> Num -> Num> Add2 { }\n"
    "         arg=<Num> NumLiteral { n= 1 }\n"
    "      }\n"
    "      f[2]=<Num -> Num> Add1 {\n"
    "         base=<Num -> Num -> Num> Add2 { }\n"
    "         arg=<Num> NumLiteral { n= 2 }\n"
    "      }\n"
    "      f[3]=<Num -> Str> Tostr1 { }\n"
    "   }\n"
    "}\n"
    "<[Str*]* -> Str*> Join1 {\n"
    // "<[Str]* -> Str*> Join1 {\n"
    "   base=<Str -> [Str] -> Str> Join2 { }\n"
    // "   base=<Str -> [Str]* -> Str*> Join2 { }\n"
    "   arg=<Str> StrLiteral { s= \":\" }\n"
    "}\n"
    "<Str* -> ()> Output { }\n"
  );

  doTestEq(
    "tonum, +1, tostr"
    ,
    "<Str*> Input { }\n"
    "<Str -> Num> Tonum1 { }\n"
    "<Num -> Num> Flip0 {\n"
    "   base=<Num -> Num -> Num> Flip1 {\n"
    "      base=<(a -> b -> c) -> b -> a -> c> Flip2 { }\n"
    "      arg=<Num -> Num -> Num> Add2 { }\n"
    "   }\n"
    "   arg=<Num> NumLiteral { n= 1 }\n"
    "}\n"
    "<Num -> Str> Tostr1 { }\n"
    "<Str* -> ()> Output { }\n"
  );

  doTestEq(
    "{1, 2, 3, :soleil:, {map}}"
    ,
    "<Str*> Input { }\n"
    "<[_mixed]> LstLiteral {\n"
    "   v[0]=<Num> NumLiteral { n= 1 }\n"
    "   v[1]=<Num> NumLiteral { n= 2 }\n"
    "   v[2]=<Num> NumLiteral { n= 3 }\n"
    "   v[3]=<Str> StrLiteral { s= \"soleil\" }\n"
    "   v[4]=<[_mixed]> LstLiteral { v[0]=<(a -> b) -> [a] -> [b]> Map2 { } }\n"
    // "   v[4]=<[_mixed]> LstLiteral { v[0]=<(a -> b) -> [a]* -> [b]*> Map2 { } }\n"
    "}\n"
    "<Str* -> ()> Output { }\n"
  );

  doTestEq(
    "%map"
    ,
    "<Str*> Input { }\n"
    "<[a] -> (a -> b) -> [b]> Flip1 {\n"
    // "<[a]* -> (a -> b) -> [b]*> Flip1 {\n"
    "   base=<(a -> b -> c) -> b -> a -> c> Flip2 { }\n"
    "   arg=<(a -> b) -> [a] -> [b]> Map2 { }\n"
    // "   arg=<(a -> b) -> [a]* -> [b]*> Map2 { }\n"
    "}\n"
    "<Str* -> ()> Output { }\n"
  );

  doTestEq(
    "tonum, %-1, tostr"
    ,
    "<Str*> Input { }\n"
    "<Str -> Num> Tonum1 { }\n"
    "<Num -> Num> Flip0 {\n"
    "   base=<Num -> Num -> Num> Flip1 {\n"
    "      base=<(a -> b -> c) -> b -> a -> c> Flip2 { }\n"
    "      arg=<Num -> Num -> Num> Flip1 {\n"
    "         base=<(a -> b -> c) -> b -> a -> c> Flip2 { }\n"
    "         arg=<Num -> Num -> Num> Sub2 { }\n"
    "      }\n"
    "   }\n"
    "   arg=<Num> NumLiteral { n= 1 }\n"
    "}\n"
    "<Num -> Str> Tostr1 { }\n"
    "<Str* -> ()> Output { }\n"
  );
}
