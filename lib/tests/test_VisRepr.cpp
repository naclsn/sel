#include "common.hpp"

void doTestEq(Val const& val, string const exp) {
  ostringstream oss;
  VisRepr repr = VisRepr(oss, {.single_line=true});
  repr(val);
  cout << "repr: " << oss.str() << endl;
  assert_cmp(exp, oss.str());
}

TEST(VisRepr) {
  auto num = NumLiteral(42.1);
  doTestEq(
    num,
    "<Num> NumLiteral { n= \"42.100000\" }"
  );

  auto str = StrLiteral("coucou");
  doTestEq(
    str,
    "<Str> StrLiteral { s= \"coucou\" }"
  );

  auto v = vector<Val*>();
  v.push_back(&num);
  v.push_back(&str);
  doTestEq(
    LstLiteral(v),
    "<[mixed]> LstLiteral { v[0]=<Num> NumLiteral { n= \"42.100000\" } v[1]=<Str> StrLiteral { s= \"coucou\" } }"
  );
}
