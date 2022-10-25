#ifndef SEL_ERROR_HPP
#define SEL_ERROR_HPP

/**
 * The various types of errors that can occure. All extend
 * from `BaseError`.
 */

#include <stdexcept>
#include <sstream>
#include <string>

#include "types.hpp"
#include "engine.hpp"

namespace sel {

  struct BaseError : std::runtime_error {
    BaseError(char const* msg): std::runtime_error(msg), w(nullptr) { }
    virtual ~BaseError() { delete w; }
  protected:
    std::string mutable* w;
    char const* what() const noexcept override;
    virtual std::ostringstream* getWhat() const = 0;
  };

  struct NIYError : BaseError {
    std::string const missing;
    NIYError(std::string const missing, char const* msg)
      : BaseError(msg)
      , missing(missing)
    { }
  protected:
    std::ostringstream* getWhat() const override;
  };

  struct ParseError : BaseError {
    std::string const* expected;
    std::string const* situation;
    ParseError(std::string const expected, std::string const situation, char const* msg)
      : BaseError(msg)
      , expected(new std::string(expected))
      , situation(new std::string(situation))
    { }
    ~ParseError() {
      delete expected;
      delete situation;
    }
  protected:
    std::ostringstream* getWhat() const override;
  };

  struct EOSError : ParseError {
    EOSError(std::string const expected, char const* msg)
      : ParseError(expected, "reached end of input", msg)
    { }
  };

  struct NameError : ParseError {
    std::string const* name;
    NameError(std::string const unknown_name, char const* msg)
      : ParseError("known name", "got unknown name '"+unknown_name+"'", msg)
      , name(new std::string(unknown_name))
    { }
    ~NameError() {
      delete name;
    }
  };

  struct TypeError : BaseError {
    TypeError(char const* msg)
      : BaseError(msg)
    { }
  };

  struct CoerseError : TypeError {
    Type const* from;
    Type const* to;
    CoerseError(Type const& from, Type const& to, char const* msg)
      : TypeError(msg)
      , from(new Type(from))
      , to(new Type(to))
    { }
    ~CoerseError() {
      delete from;
      delete to;
    }
  protected:
    std::ostringstream* getWhat() const override;
  };

  struct ParameterError : TypeError {
    Val const* many;
    ParameterError(char const* msg)
      : TypeError(msg)
      , many(nullptr)
    { }
    ParameterError(Val* many, char const* msg)
      : TypeError(msg)
      , many(many)
    { }
    // ~ParameterError() {
    //   delete many;
    // }
  protected:
    std::ostringstream* getWhat() const override;
  };
} // namespace sel

#endif // SEL_ERROR_HPP
