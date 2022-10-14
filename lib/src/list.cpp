#include "list.hpp"
#include "error.hpp"

namespace sel {

  std::ostream& Lst::output(std::ostream& out) {
    char const* sep = Ty::LST == type().pars.pair[0]->base
      ? Ty::LST == type().pars.pair[0]->pars.pair[0]->base
        ? "\n\n"
        : "\n"
      : " ";
    bool first = true;
    while (!end())
      out << (first ? "" : sep) << next();
    return out;
  }
  Val& Lst::operator[](size_t n) {
    rewind();
    for (size_t k = 0; k < n; k++) next();
    return next();
  }


  Val* Lst::coerse(Type to) {
    if (Ty::LST == to.base)
      throw CoerseError(Ty::NUM, to, "Num::coerse");
    TODO("recursive list coersion");
    PANIC("not done");
  }

}
