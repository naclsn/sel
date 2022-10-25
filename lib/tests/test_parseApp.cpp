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
    "split : :, map [tonum, add 1, add 2, tostr], join ::::",
    "<() -> Str> Stdin {}\n"
    "<Str* -> [Str]*> Split1 {\n"
    "   base=<Str -> Str* -> [Str]*> Split2 {}\n"
    "   arg=<Str> StrLiteral {s= \" \"}\n"
    "}\n"
    "<[Str]* -> [Str]*> Map1 {\n"
    "   base=<(a -> b) -> [a]* -> [b]*> Map2 {}\n"
    "   arg=<Str -> Str> FunChain {\n"
    "      f[0]=<Str -> Num> Tonum1 {}\n"
    "      f[1]=<Num -> Num> Add1 {\n"
    "         base=<Num -> Num -> Num> Add2 {}\n"
    "         arg=<Num> NumLiteral {n= \"1.000000\"}\n"
    "      }\n"
    "      f[2]=<Num -> Num> Add1 {\n"
    "         base=<Num -> Num -> Num> Add2 {}\n"
    "         arg=<Num> NumLiteral {n= \"2.000000\"}\n"
    "      }\n"
    "      f[3]=<Num -> Str> Tostr1 {}\n"
    "   }\n"
    "}\n"
    "<[Str]* -> Str*> Join1 {\n"
    "   base=<Str -> [Str]* -> Str*> Join2 {}\n"
    "   arg=<Str> StrLiteral {s= \":\"}\n"
    "}\n"
    "<Str* -> ()> Stdout {}\n"
  );
}
