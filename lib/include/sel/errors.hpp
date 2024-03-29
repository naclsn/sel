#ifndef SEL_ERROR_HPP
#define SEL_ERROR_HPP

/**
 * The various types of errors that can occure:
 * BaseError, ParseError, TypeError, RuntimeError.
 * NIYError is kept for now as a wildcard
 */

#include <sstream>
#include <stdexcept>
#include <string>

#include "types.hpp"

namespace sel {

  struct Val;

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
    size_t start, span;
    ParseError(std::string const& msg, size_t start, size_t span)
      : BaseError(msg)
      , start(start)
      , span(span)
    { }
  };

  struct TypeError : BaseError {
    // size_t start, span;
    TypeError(std::string const& msg) // range..
      : BaseError(msg)
    { }
    TypeError(Val const& from, Type const& to) // range..
      : TypeError(bidoof(from, to))
    { }
  private:
    static std::string bidoof(Val const& from, Type const& to);
  };

  struct RuntimeError : BaseError {
    RuntimeError(std::string const& msg)
      : BaseError(msg)
    { }
  };

} // namespace sel

#endif // SEL_ERROR_HPP
