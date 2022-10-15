#ifndef SEL_ERROR_HPP
#define SEL_ERROR_HPP

#include <iostream>

#include "types.hpp"

namespace sel {

  struct TypeError : public std::runtime_error {
  public:
    TypeError(char const* msg): std::runtime_error(msg) { }
  };

  struct CoerseError : public TypeError {
  public:
    const Type from;
    const Type to;

    CoerseError(Type from, Type to, char const* msg): TypeError(msg), from(from), to(to) { }
  };

  // struct ParameterError : public TypeError {
  // public:
  //   const Val* many;
  
  //   ParameterError(char const* msg): TypeError(msg), many(nullptr) { }
  //   ParameterError(Val* many, char const* msg): TypeError(msg), many(many) { }
  // };
}

#endif // SEL_ERROR_HPP
