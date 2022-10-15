#ifndef SEL_TYPE_HPP
#define SEL_TYPE_HPP

/**
 * Parsing and representation of types, as well as a set
 * of helper functions to deal with the type system.
 */

#include <ostream>
#include <string>

#include "utils.hpp"

namespace sel {

  enum class Ty {
    UNK, // str
    NUM, // 0
    STR, // 0
    LST, // 1
    FUN, // 2
    CPL, // 2
  };
  enum TyFlag {
    IS_FIN = 0,
    IS_INF = 1,
    // IS_SIMPLE = 2, // no complex nested type(s), no dynamic alloc
  };

  /**
   * Represents a type (yey). Use the [..]Type function
   * to construct/parse.
   */
  struct Type {
    Ty base;
    union {
      std::string* name;
      Type* box_has; // Ty has;
      Type* box_pair[2]; // Ty pair[2];
    } p = {0};
    uint8_t flags = 0;

    // private: Type() { } // YYY: would like to disable default ctor
    ~Type();

    bool operator==(Type const& other) const;
  }; // struct Type

  Type unkType(std::string* name);
  Type numType();
  Type strType(TyFlag is_inf);
  Type lstType(Type* has, TyFlag is_inf);
  Type funType(Type* fst, Type* snd);
  Type cplType(Type* fst, Type* snd);

  /**
   * Parse a type from the given stream. The overload to
   * the `>>` operator is also provided as a convenience,
   * but this function allows retreiving a name
   * (which would be specified with the `<name> ::`
   * syntax). If `named` is not nullptr but the parsed
   * type representation does not specify a name, the
   * empty string is written into `named`.
  */
  void parseType(std::istream& in, std::string* named, Type& res);

  std::ostream& operator<<(std::ostream& out, Type const& ty);
  std::istream& operator>>(std::istream& in, Type& tt);

} // namespace sel

#endif // SEL_TYPE_HPP
