#include "number.hpp"
#include "coerse.hpp"
#include "error.hpp"

namespace sel {

  Val* Num::coerse(Type to) {
    if (Ty::NUM == to.base) return this;
    if (Ty::STR == to.base) {
      Val* r = new StrFromNum(this);
      return r;
    }
    throw CoerseError(Ty::NUM, to, "Num::coerse");
  }

  void NumFloat::eval() { }
  std::ostream& NumFloat::output(std::ostream& out) {
    return out << n;
  }
  // Val* NumFloat::clone() const {
  //   return new NumFloat(n);
  // }

}
