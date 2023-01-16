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
    Type(Ty::LST,
      {.box_has= new vector<Type*>{
        new Type(Ty::UNK, {.name=new std::string("a")}, 0),
        new Type(Ty::UNK, {.name=new std::string("b")}, 0),
      }}, TyFlag::IS_TPL
    )
  )+

  doTestEq(
    "ty :: [a, b]*",
    Type(Ty::LST,
      {.box_has= new vector<Type*>{
        new Type(Ty::UNK, {.name=new std::string("a")}, 0),
        new Type(Ty::UNK, {.name=new std::string("b")}, 0),
      }}, TyFlag::IS_INF
    )
  )+

  doTestEq(
    "ty :: (Num -> Str*) -> Num -> Num",
    Type(Ty::FUN,
      {.box_pair={
        new Type(Ty::FUN,
          {.box_pair={
            new Type(Ty::NUM, {0}, 0),
            new Type(Ty::STR, {0}, TyFlag::IS_INF)
          }}, 0
        ),
        new Type(Ty::FUN,
          {.box_pair={
            new Type(Ty::NUM, {0}, 0),
            new Type(Ty::NUM, {0}, 0)
          }}, 0
        )
      }}, 0
    )
  )+

  0;
}
