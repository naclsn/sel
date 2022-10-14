#include "string.hpp"
#include "error.hpp"

namespace sel {

  Val* Str::coerse(Type to) {
    if (Ty::STR == to.base) return this;
    if (Ty::NUM == to.base) {
      // Val* r = new CoerseStrToNum((Val*)this)
    }
    throw CoerseError(type(), to, "Str::coerse");
  }

  void StrBuffered::eval() {
    if (!hasBuffer())
      buffer = new std::string("");
  }
  std::ostream& StrBuffered::output(std::ostream& out) {
    return out << *buffer;
  }
  // Val* StrBuffered::clone() const {
  //   return new StrBuffered(*buffer);
  // }

  void StrFromStream::eval() {
    if (hasBuffer()) return;

    char cbuf[4096];
    std::string tmp;
    while (stream->read(cbuf, sizeof(cbuf)))
      tmp.append(cbuf, sizeof(cbuf));
    tmp.append(cbuf, stream->gcount());

    setBuffer(new std::string(tmp));
  }
  // Val* StrFromStream::clone() const {
  //   if (hasBuffer()) return StrBuffered::clone();
  //   TODO("cloning a stream makes no sense"); // return new StrFromStream(stream);
  //   return StrBuffered::clone();
  // }

}
