#include "coerse.hpp"

namespace sel {

//#pragma region CoreseNumToStr
  Val* CoerseNumToStr::eval() const {
    if (nullptr == result) {
      // Num const* trueNum = source->as<Num>();
      std::stringstream* str = new std::stringstream();
      *str << source;
      result = new Str(str);
    }
    return result;
  }

  const Val* CoerseNumToStr::coerse(Type to) const {
    if (nullptr == result) {
      // coersing back to original type
      if (to == source->typed()) {
        auto previous = source;
        source = nullptr;
        delete this;
        return previous;
      }
      // coersing into a new type (no need for this one to take place anymore)
      throw TypeError(typed(), to, "CoerseNumToStr::coerse");
    }
    throw TypeError(typed(), to, "CoerseNumToStr::coerse");
  }
//#pragma endregion

} // namespace sel
