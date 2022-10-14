#include "function.hpp"
#include "error.hpp"

namespace sel {

  Val* Fun::coerse(Type to) {
    throw CoerseError(type(), to, "Fun::coerse");
  }

}
