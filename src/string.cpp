#include "string.hpp"
#include "error.hpp"

namespace sel {

  Val* Str::coerse(Type to) {
    if (Ty::STR == to.base) return this;
    if (Ty::NUM == to.base) {
      // Val* r = new CoerseStrToNum((Val*)this)
    }
    throw CoerseError(typed(), to, "Str::coerse");
  }

  void StrFromStream::eval() {
    if (hasBuffer()) return;

    char cbuf[4096];
    std::string tmp;
    while (stream->read(cbuf, sizeof(cbuf)))
      tmp.append(cbuf, sizeof(cbuf));
    tmp.append(cbuf, stream->gcount());

    setBuffer(new std::string(tmp));
  }

}
