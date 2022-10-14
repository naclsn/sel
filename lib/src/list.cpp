#include "list.hpp"
#include "error.hpp"

namespace sel {

  Val* Lst::coerse(Type to) {
    if (Ty::LST == to.base)
      throw CoerseError(Ty::NUM, to, "Num::coerse");
    TODO("recursive list coersion");
    PANIC("not done");
  }

}
