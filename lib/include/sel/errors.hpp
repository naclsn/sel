#ifndef SEL_ERROR_HPP
#define SEL_ERROR_HPP

/**
 * The various types of errors that can occure:
 * BaseError, ParseError, TypeError, RuntimeError.
 * NIYError is kept for now as a wildcard
 */

#include <stdexcept>
#include <sstream>
#include <string>

#include "types.hpp"
#include "engine.hpp"

namespace sel {

  struct BaseError : std::runtime_error {
    BaseError(std::string const& msg)
      : std::runtime_error(msg)
    { }
  };

  struct NIYError : BaseError {
    NIYError(std::string const&  missing)
      : BaseError("not implemented yet: " + missing)
    { }
  };

  struct ParseError : BaseError {
    ParseError(std::string const& msg)
      : BaseError(msg)
    { }
  };

  struct TypeError : BaseError {
    TypeError(std::string const& msg)
      : BaseError(msg)
    { }
  };

  struct RuntimeError : BaseError {
    RuntimeError(std::string const& msg)
      : BaseError(msg)
    { }
  };

} // namespace sel

#endif // SEL_ERROR_HPP
