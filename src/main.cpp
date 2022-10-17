#include <iostream>
#include <sstream>

#include "sel/visitors.hpp"
#include "sel/crap.hpp"

using namespace std;
using namespace sel;

int main() {
  ValRepr repr = ValRepr(cout, {});
  Crap crap = Crap();

  crap.accept(repr);
  // repr(crap);

  return 0;
}
