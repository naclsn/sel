#include "common.hpp"

int doTestEq(char const* source, char const* repr) {
  istringstream iss(source);
  App app;
  iss >> app;

  cout << "successfully parsed source '" << source << "'" << endl;

  ostringstream oss;
  app.repr(oss);

  cout << "application representation:\n" << oss.str() << endl;
  assert_cmp(repr, oss.str());

  return 0;
}

TEST(parseApp) {
  return

  doTestEq(
    "split : :, map [tonum, add 1, add 2, tostr], join ::::"
    ,
    "App {\n"
    "   f= <Str* -> Str*> FunChain {\n"
    "      f[0]=<Str* -> [Str*]*> Split1 { arg_A=<Str> StrLiteral { s= \" \" } }\n"
    "      f[1]=<[Str*]* -> [Str]*> Map1 { arg_A=<Str* -> Str> FunChain {\n"
    "         f[0]=<Str* -> Num> Tonum1 { }\n"
    "         f[1]=<Num -> Num> Add1 { arg_A=<Num> NumLiteral { n= 1 } }\n"
    "         f[2]=<Num -> Num> Add1 { arg_A=<Num> NumLiteral { n= 2 } }\n"
    "         f[3]=<Num -> Str> Tostr1 { }\n"
    "      } }\n"
    "      f[2]=<[Str*]* -> Str*> Join1 { arg_A=<Str> StrLiteral { s= \":\" } }\n"
    "   }\n"
    "   user= {}\n"
    "}\n"
  )+

  doTestEq(
    "tonum, +1, tostr"
    ,
    "App {\n"
    "   f= <Str* -> Str> FunChain {\n"
    "      f[0]=<Str* -> Num> Tonum1 { }\n"
    "      f[1]=<Num -> Num> Flip0 {\n"
    "         arg_A=<Num -> Num -> Num> Add2 { }\n"
    "         arg_B=<Num> NumLiteral { n= 1 }\n"
    "      }\n"
    "      f[2]=<Num -> Str> Tostr1 { }\n"
    "   }\n"
    "   user= {}\n"
    "}\n"
  )+

  doTestEq(
    "const {1, 2, 3, :soleil:, {map}}"
    ,
    "App {\n"
    "   f= <b -> [_mixed]> Const0 { arg_A=<[_mixed]> LstLiteral {\n" // XXX: _mixed
    "      v[0]=<Num> NumLiteral { n= 1 }\n"
    "      v[1]=<Num> NumLiteral { n= 2 }\n"
    "      v[2]=<Num> NumLiteral { n= 3 }\n"
    "      v[3]=<Str> StrLiteral { s= \"soleil\" }\n"
    "      v[4]=<[_mixed]> LstLiteral { v[0]=<(a -> b) -> [a]* -> [b]*> Map2 { } }\n"
    "   } }\n"
    "   user= {}\n"
    "}\n"
  )+

  doTestEq(
    "const {}, reverse, join : :"
    ,
    "App {\n"
    "   f= <b -> Str*> FunChain {\n"
    "      f[0]=<b -> [_mixed]> Const0 { arg_A=<[_mixed]> LstLiteral { } }\n" // XXX: _mixed
    "      f[1]=<[a] -> [a]> Reverse1 { }\n"
    "      f[2]=<[Str*]* -> Str*> Join1 { arg_A=<Str> StrLiteral { s= \" \" } }\n"
    "   }\n"
    "   user= {}\n"
    "}\n"
  )+

  doTestEq(
    "const {1}, reverse, join : :"
    ,
    "App {\n"
    "   f= <b -> Str*> FunChain {\n"
    "      f[0]=<b -> [_mixed]> Const0 { arg_A=<[_mixed]> LstLiteral { v[0]=<Num> NumLiteral { n= 1 } } }\n" // XXX: _mixed -> Num
    "      f[1]=<[a] -> [a]> Reverse1 { }\n"
    "      f[2]=<[Str*]* -> Str*> Join1 { arg_A=<Str> StrLiteral { s= \" \" } }\n"
    "   }\n"
    "   user= {}\n"
    "}\n"
  )+

  doTestEq(
    "const {1, 2}, reverse, join : :"
    ,
    "App {\n"
    "   f= <b -> Str*> FunChain {\n"
    "      f[0]=<b -> [_mixed]> Const0 { arg_A=<[_mixed]> LstLiteral {\n" // XXX: _mixed -> Num
    "         v[0]=<Num> NumLiteral { n= 1 }\n"
    "         v[1]=<Num> NumLiteral { n= 2 }\n"
    "      } }\n"
    "      f[1]=<[a] -> [a]> Reverse1 { }\n"
    "      f[2]=<[Str*]* -> Str*> Join1 { arg_A=<Str> StrLiteral { s= \" \" } }\n"
    "   }\n"
    "   user= {}\n"
    "}\n"
  )+

  doTestEq(
    "%map"
    ,
    "App {\n"
    "   f= <[a]* -> (a -> b) -> [b]*> Flip1 { arg_A=<(a -> b) -> [a]* -> [b]*> Map2 { } }\n"
    "   user= {}\n"
    "}\n"
  )+

  doTestEq(
    "tonum, %-1, tostr"
    ,
    "App {\n"
    "   f= <Str* -> Str> FunChain {\n"
    "      f[0]=<Str* -> Num> Tonum1 { }\n"
    "      f[1]=<Num -> Num> Flip0 {\n"
    "         arg_A=<Num -> Num -> Num> Flip1 { arg_A=<Num -> Num -> Num> Sub2 { } }\n"
    "         arg_B=<Num> NumLiteral { n= 1 }\n"
    "      }\n"
    "      f[2]=<Num -> Str> Tostr1 { }\n"
    "   }\n"
    "   user= {}\n"
    "}\n"
  )+

  0;
}
