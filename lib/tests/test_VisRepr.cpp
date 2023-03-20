#include "common.hpp"

int doTestEq(Val const& val, string const exp) {
  ostringstream oss;
  VisRepr repr(oss, {.single_line=true});
  val.accept(repr);
  cout << "repr: " << oss.str() << endl;
  assert_cmp(exp, oss.str());

  return 0;
}

App app;

TEST(VisRepr) {
  int fails = 0;

  auto num = make_unique<NumLiteral>(42.1);
  fails+= doTestEq(
    *num,
    "<Num> NumLiteral { n= 42.1 }"
  );

  auto str = make_unique<StrLiteral>("coucou");
  fails+= doTestEq(
    *str,
    "<Str> StrLiteral { s= \"coucou\" }"
  );

  Vals v;
  v.push_back(move(num));
  v.push_back(move(str));
  fails+= doTestEq(
    *make_unique<LstLiteral>(move(v)),
    "<[_mixed]> LstLiteral { v[0]=<Num> NumLiteral { n= 42.1 } v[1]=<Str> StrLiteral { s= \"coucou\" } }"
  );

  return fails;
}
