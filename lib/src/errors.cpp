#include "sel/errors.hpp"

namespace sel {

  char const* BaseError::what() const noexcept {
    if (!w) {
      auto x = getWhat();
      w = new std::string(x->str());
      delete x;
    }
    return w->c_str();
  }

  // std::ostringstream* BaseError::getWhat() const {
  //   auto* r = new std::ostringstream();
  //   *r << "garbo";
  //   return r;
  // }

  std::ostringstream* NIError::getWhat() const {
    auto* r = new std::ostringstream();
    *r << "not implemented yet: " << missing;
    return r;
  }

  std::ostringstream* ParseError::getWhat() const {
    auto* r = new std::ostringstream();
    *r << "expected " << *expected << " but " << *situation;
    return r;
  }

  std::ostringstream* CoerseError::getWhat() const {
    auto* r = new std::ostringstream();
    *r << "cannot coerse from " << *from << " to " << *to;
    return r;
  }

  std::ostringstream* ParameterError::getWhat() const {
    auto* r = new std::ostringstream();
    *r << "got an unexpected parameter of type " << many->type();
    return r;
  }

} // namespace sel
