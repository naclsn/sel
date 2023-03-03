#include "common.hpp"

int doTestEq(char const* source, Type const& expect) {
  istringstream iss(source);
  Type result;
  string name;
  parseType(iss, &name, result);

  cout
    << "- source: " << source << endl
    << "- expect: ty :: " << expect << endl
    << "- result: " << name << " :: " << result << endl
    << "compares equal: " << (expect == result) << endl
  ;

  assert_eq(expect, result);

  ostringstream oss;
  oss << name << " :: " << result;
  string a = source;
  string b = oss.str();
  assert_eq(a, b);

  return 0;
}

TEST(parseType) {
  return

  doTestEq(
    "ty :: (a, b)",
    Type::makeLst({
      Type::makeUnk("a"),
      Type::makeUnk("b"),
    }, false, true)
  )+

  doTestEq(
    "ty :: [a, b]*",
    Type::makeLst({
      Type::makeUnk("a"),
      Type::makeUnk("b"),
    }, false, false)
  )+

  doTestEq(
    "ty :: (Num -> Str*) -> Num -> Num",
    Type::makeFun(
      Type::makeFun(
        Type::makeNum(),
        Type::makeStr(true)
      ),
      Type::makeFun(
        Type::makeNum(),
        Type::makeNum()
      )
    )
  )+

  0;
}
