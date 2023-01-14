#include "common.hpp"

void doTestEq(Val const& val, string const exp) {
  ostringstream oss;
  VisRepr repr = VisRepr(oss, {.single_line=true});
  repr(val);
  cout << "repr: " << oss.str() << endl;
  assert_cmp(exp, oss.str());
}

App app;

TEST(VisRepr) {
  auto num = NumLiteral(app, 42.1);
  doTestEq(
    num,
    "<Num> NumLiteral { n= 42.1 }"
  );

  auto str = StrLiteral(app, "coucou");
  doTestEq(
    str,
    "<Str> StrLiteral { s= \"coucou\" }"
  );

  auto v = vector<Val*>();
  v.push_back(&num);
  v.push_back(&str);
  doTestEq(
    LstLiteral(app, v),
    "<[_mixed]> LstLiteral { v[0]=<Num> NumLiteral { n= 42.1 } v[1]=<Str> StrLiteral { s= \"coucou\" } }"
  );
}
