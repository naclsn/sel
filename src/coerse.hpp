#ifndef SEL_COERSE_HPP
#define SEL_COERSE_HPP

#include <iostream>
#include <sstream>

#include "value.hpp"

namespace sel {

  class CoerseNumToStr : public Val {
  private:
    mutable Val const* source;
    mutable Val* result;

  protected:
    Val* eval() const override;
    std::ostream& output(std::ostream& out) const override { return out << *eval(); }

  public:
    CoerseNumToStr(Val const* source): Val(BasicType::STR), source(source) { }
    ~CoerseNumToStr() {
      std::cerr << "~CoerseNumToStr()" << std::endl;
      delete source;
      delete result;
    }

    // YYY: idk if as<> realy used
    // template <typename T>
    // T const* as() const;
    // template <>
    // Num const* as<Num>() const { return (Num*)source; }

    Val const* coerse(Type to) const override;
  }; // class CoerseNumToStr

} // namespace sel

#endif // SEL_COERSE_HPP
