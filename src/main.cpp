#include <iostream>
#include <sstream>

#include "errors.hpp"
#include "types.hpp"

using namespace std;
using namespace sel;

void test_parseType() {
  char const* source = "fn :: (Num -> ((Str))*) -> Num -> ([Str]*, [Str*])";

  Type expect = funType(
    new Type(funType(
      new Type(numType()),
      new Type(strType(TyFlag::IS_INF))
    )),
    new Type(funType(
      new Type(numType()),
      new Type(cplType(
        new Type(lstType(new Type(strType(TyFlag::IS_FIN)), TyFlag::IS_INF)),
        new Type(lstType(new Type(strType(TyFlag::IS_INF)), TyFlag::IS_FIN))
      ))
    ))
  );

  std::istringstream ss(source);
  Type result;
  std::string name;
  parseType(ss, &name, result);

  cout
    << "  source: " << source << endl
    << "  expect: fn :: " << expect << endl
    << "  result: " << name << " :: " << result << endl
    << "compares equal: " << (expect == result) << endl
  ;
}

int main() {
  try {
    test_parseType();
  } catch (std::string const& e) {
    cerr << "got err literal: " << e << endl;
  } catch (ParseError const& e) {
    cerr << "parse error: expected " << *e.expected << "\n  situation: " << *e.situation << endl;
  }
  return 0;
}
