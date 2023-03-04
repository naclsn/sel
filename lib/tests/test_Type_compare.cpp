#include "common.hpp"

int eq(char const* r, char const* l) {
  Type tya;
  Type tyb;
  istringstream issa(r);
  istringstream issb(l);
  issa >> tya;
  issb >> tyb;
  assert_eq(tya, tyb);
  assert_eq(tyb, tya);
  return 0;
}

int ne(char const* r, char const* l) {
  Type tya;
  Type tyb;
  istringstream issa(r);
  istringstream issb(l);
  issa >> tya;
  issb >> tyb;
  assert_ne(tya, tyb);
  assert_ne(tyb, tya);
  return 0;
}

TEST(Type_compare) {
  return
  eq("Num", "Num")+
  eq("Str", "Str")+
  ne("Str", "Num")+

  eq("a", "a")+
  eq("a", "b")+

  eq("[Num]", "[Num]")+
  eq("a", "[Num]")+
  eq("[Str]", "b")+
  ne("(a, a)", "(Num, Str)")+

  eq("Str -> Num", "a -> b")+
  eq("Str -> Str", "a -> a")+
  ne("Str -> Num", "a -> a")+

  // YYY: looks like it should not, but it logically does
  // ne("Str -> Str", "a -> b")+
  // ne("[a]", "a")+
  0;
}
