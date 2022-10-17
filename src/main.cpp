#include <iostream>
#include <sstream>

#include "sel/visitors.hpp"
#include "sel/bidoof.hpp"

using namespace std;
using namespace sel;

int main() {
  std::ostringstream oss;
  ValRepr repr = ValRepr(oss, {});
  Bidoof bidoof = Bidoof();

  bidoof.accept(repr);
  // repr(bidoof);

  cout << oss.str() << "\n";

  return 0;
}
