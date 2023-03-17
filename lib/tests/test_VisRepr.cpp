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

  auto num = handle<NumLiteral>(app, 42.1);
  fails+= doTestEq(
    *num,
    "<Num> NumLiteral { n= 42.1 }"
  );

  auto str = handle<StrLiteral>(app, "coucou");
  fails+= doTestEq(
    *str,
    "<Str> StrLiteral { s= \"coucou\" }"
  );

  auto v = vector<handle<Val>>();
  v.push_back(num);
  v.push_back(str);
  fails+= doTestEq(
    *handle<LstLiteral>(app, v),
    "<[_mixed]> LstLiteral { v[0]=<Num> NumLiteral { n= 42.1 } v[1]=<Str> StrLiteral { s= \"coucou\" } }"
  );

  return fails;
}
