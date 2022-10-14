#include "coerse.hpp"

namespace sel {

  void StrFromNum::eval() {
    if (!hasBuffer()) {
      std::stringstream* str = new std::stringstream();
      *str << *source;
      setStream(str);
    }
    StrFromStream::eval();
  }
  // Val* StrFromNum::clone() {
  //   return new StrFromNum((Num*)source->clone());
  // }
  Val* StrFromNum::coerse(Type to) {
    if (!hasBuffer()) {
      if (Ty::NUM == to.base) {
        Num* previous = source;
        source = nullptr; // as to not free it
        delete this;
        return previous;
      }
    }
    return StrFromStream::coerse(to);
  }

} // namespace sel
