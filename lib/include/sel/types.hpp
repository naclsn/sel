#ifndef SEL_TYPE_HPP
#define SEL_TYPE_HPP

/**
 * Parsing and representation of types, as well as a set
 * of helper functions to deal with the type system.
 */

#include <ostream>
#include <istream>
#include <string>
#include <vector>
#include <iterator>


namespace sel {

  enum class Ty {
    UNK,
    NUM,
    STR,
    LST,
    FUN,
  };
  enum TyFlag {
    IS_FIN = 0, // (0, and as such default)
    IS_INF = 1,
    IS_TPL = 2,
  };

  /**
   * Represents a type (yey). Use the [..]Type function
   * to construct/parse.
   */
  struct Type { // TODO: maybe could do with cleaner move 'compliance' (eg. operator=) ((or none at all))
    Ty base = Ty::UNK;
    union P {
      std::string* name;
      std::vector<Type*>* box_has; // Ty has;
      Type* box_pair[2]; // Ty pair[2];
    } p = {.name=nullptr};
    uint8_t flags = 0;

    Type() { }
    Type(Ty base, Type::P p, uint8_t flags);
    Type(Type const& ty);
    Type(Type&& ty) noexcept;
    ~Type();

    bool operator==(Type const& other) const;
    bool operator!=(Type const& other) const;

    std::vector<Type*>& has() const { return *p.box_has; }
    Type const& from() const { return *p.box_pair[0]; }
    Type const& to() const { return *p.box_pair[1]; }
    Type applied(Type const& arg);
  };

  // TODO: remove both
  std::vector<Type*>* types1(Type* ty1);
  std::vector<Type*>* types2(Type* ty1, Type* ty2);

  // Type unkType(std::string* name);
  // Type numType();
  // Type strType(TyFlag is_inf);
  // Type lstType(Type* has, TyFlag is_inf);
  // Type funType(Type* fst, Type* snd);

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
  std::istream& operator>>(std::istream& in, Type& res);

} // namespace sel

#endif // SEL_TYPE_HPP
