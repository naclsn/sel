#include <iostream>
#include <typeinfo>

#include "sel/errors.hpp"
#include "sel/engine.hpp"
#include "sel/utils.hpp"
#include "sel/builtins.hpp"

namespace sel {

  void Val::accept(Visitor& v) const {
    throw NIYError(std::string("'accept' of visitor pattern for this class: ") + typeid(*this).name());
  }

  // ZZZ
  template <typename T> struct _type_str;
  template <> struct _type_str<Val> { constexpr static char const* the = "Val"; };
  template <> struct _type_str<Num> { constexpr static char const* the = "Num"; };
  template <> struct _type_str<Str> { constexpr static char const* the = "Str"; };
  template <> struct _type_str<Lst> { constexpr static char const* the = "Lst"; };
  template <> struct _type_str<Fun> { constexpr static char const* the = "Fun"; };
  template <typename To>
  void _coerse_info(Val* from, Type const& to) {
    std::cerr
      << "coerse<" << _type_str<To>::the << ">(\n"
      << "   " << from->type() << ",\n"
      << "   " << to << "\n"
      << ")\n"
    ;
  }

  template <typename To>
  To* coerse(Val* from, Type const& to);

  // not much to be done, this is used in bins for unknowns
  template <> Val* coerse<Val>(Val* from, Type const& to) {
    _coerse_info<Val>(from, to);
    return from;
  }

  // Str => Num: same as `tonum`
  template <> Num* coerse<Num>(Val* from, Type const& to) {
    _coerse_info<Num>(from, to);
    Type const& ty = from->type();
    if (Ty::NUM == ty.base) return (Num*)from;

    if (Ty::STR == ty.base)
      return (Num*)(*static_lookup_name(tonum))(from);

    throw TypeError(ty, to);
  }

  // Num => Str: same as `tostr`
  // [Str] => Str: same as `join::` (discussed, will not implement yet)
  template <> Str* coerse<Str>(Val* from, Type const& to) {
    _coerse_info<Str>(from, to);
    Type const& ty = from->type();
    if (Ty::STR == ty.base) return (Str*)from;

    if (Ty::NUM == ty.base)
      return (Str*)(*static_lookup_name(tostr))(from);
    if (Ty::LST == ty.base && 0 < ty.p.box_has->size() && Ty::STR == ty.p.box_has->at(0)->base) // and that's not even enough!
      throw NIYError("coersion [Str] => Str");

    throw TypeError(ty, to);
  }

  // Str => [Num]: same as `codepoints`
  // Str => [Str]: same as `graphems`
  template <> Lst* coerse<Lst>(Val* from, Type const& to) {
    _coerse_info<Lst>(from, to);
    Type const& ty = from->type();

    if (Ty::LST == ty.base) {
      // TODO: need to recurse
      return (Lst*)from;
    }

    if (Ty::STR == ty.base) {
      if (Ty::STR == to.p.box_has->at(0)->base) // YYY: no, that not good
        throw NIYError("coersion Str => [Str]");
      if (Ty::NUM == to.p.box_has->at(0)->base) // YYY: no, that not good
        throw NIYError("coersion Str => [Num]");
    }

    throw TypeError(ty, to);
  }

  // special case for type checking, (effectively a cast)
  // where `to` should be `tyor idk
  template <> Fun* coerse<Fun>(Val* from, Type const& to) {
    _coerse_info<Fun>(from, to);
    if (Ty::FUN == to.base) return (Fun*)from;

    std::ostringstream oss;
    throw TypeError((oss << "value of type " << to << " is not a function", oss.str()));
  }

} // namespace sel
