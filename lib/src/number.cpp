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

  Val* NumFloat::clone() {
    return new NumFloat(n);
  }

}
