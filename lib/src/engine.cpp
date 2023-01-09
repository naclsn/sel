#include <iostream>
#include <typeinfo>

#define TRACE(...)
#include "sel/errors.hpp"
#include "sel/engine.hpp"
#include "sel/utils.hpp"
#include "sel/builtins.hpp"

namespace sel {

  void Val::accept(Visitor& v) const {
    throw NIYError(std::string("'accept' of visitor pattern for this class: ") + typeid(*this).name());
  }

  template <typename To>
  To* coerse(Val* from, Type const& to);

  // Str => Num: same as `tonum`
  template <> Num* coerse<Num>(Val* from, Type const& to) {
    TRACE(coerse<Num>
      , "from: " << from->type()
      , "to: " << to
      );
    Type const& ty = from->type();
    if (Ty::NUM == ty.base) return (Num*)from;

    if (Ty::STR == ty.base)
      return (Num*)(*static_lookup_name(tonum))(from);

    throw TypeError(ty, to);
  }

  // Num => Str: same as `tostr`
  // [Str] => Str: same as `join::` (discussed, will not implement yet)
  template <> Str* coerse<Str>(Val* from, Type const& to) {
    TRACE(coerse<Str>
      , "from: " << from->type()
      , "to: " << to
      );
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
    TRACE(coerse<Lst>
      , "from: " << from->type()
      , "to: " << to
      );
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
    TRACE(coerse<Fun>
      , "from: " << from->type()
      , "to: " << to
      );
    if (Ty::FUN == to.base) return (Fun*)from;

    std::ostringstream oss;
    throw TypeError((oss << "value of type " << to << " is not a function", oss.str()));
  }

  // dispatches to the correct one dynamically
  // coersing to unk is used in builtins (eg. `const` or `id`)
  template <> Val* coerse<Val>(Val* from, Type const& to) {
    TRACE(coerse<Val>
      , "from: " << from->type()
      , "to: " << to
      );
   switch (to.base) {
      case Ty::UNK: return from;
      case Ty::NUM: return coerse<Num>(from, to);
      case Ty::STR: return coerse<Str>(from, to);
      case Ty::LST: return coerse<Lst>(from, to);
      case Ty::FUN: return coerse<Fun>(from, to);
    }
    throw TypeError("miss-initialized or corrupted type");
  }


  Str_streambuf& Str_streambuf::operator=(Str_streambuf&& sis) {
    if (this == &sis) return *this;
    // YYY: this moves the get buffer? do i need to update it explicitly
    std::streambuf::operator=(std::move(sis));
    v = sis.v;
    buffered = std::move(sis.buffered);
    return *this;
  }

  Str_streambuf::int_type Str_streambuf::overflow(int_type) {
    return traits_type::eof();
  }

  Str_streambuf::int_type Str_streambuf::underflow() {
    // should never happend (according to some random standard)
    if (gptr() < egptr()) return *gptr();

    if (v->end()) return traits_type::eof();

    std::ostringstream oss;
    oss << *v;
    buffered = oss.str();

    char_type* cstr = (char_type*)(buffered.c_str());
    // why you no take const?
    setg(cstr, cstr, cstr+buffered.length());

    return buffered[0];
  }

  Str_istream& Str_istream::operator=(Str_istream&& sis) {
    if (this == &sis) return *this;
    std::istream::operator=(std::move(sis));
    a = std::move(sis.a);
    rdbuf(&a);
    return *this;
  }

} // namespace sel
