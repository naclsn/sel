#ifndef SEL_COERSE_HPP
#define SEL_COERSE_HPP

#include <iostream>
#include <sstream>

#include "value.hpp"
#include "number.hpp"
#include "string.hpp"
#include "error.hpp"

namespace sel {

  class StrFromNum : public StrFromStream {
  private:
    mutable Num* source;

  protected:
    void eval() override;

  public:
    StrFromNum(Num* source): source(source) { }
    VERB_DTOR(StrFromNum, {
      delete source;
    });

    Val* coerse(Type to) override;
  }; // class CoerseNumToStr

} // namespace sel

#endif // SEL_COERSE_HPP
