#include <iostream>

#include "sel/application.hpp"
#include "sel/builtins.hpp"
#include "sel/engine.hpp"
#include "sel/errors.hpp"
#include "sel/parser.hpp"
#include "sel/utils.hpp"
#include "sel/visitors.hpp"

using namespace std;

namespace sel {

  template <typename To>
  unique_ptr<To> coerse(unique_ptr<Val> from, Type const& to);

  // Str => Num: same as `tonum`
  template <> unique_ptr<Num> coerse<Num>(unique_ptr<Val> from, Type const& to) {
    Type const& ty = from->type();
    // if (app.is_strict_type() && to != ty) throw TypeError(ty, to);
    if (Ty::NUM == ty.base()) return val_cast<Num>(move(from));

    if (Ty::STR == ty.base())
      return val_cast<Num>((*static_lookup_name(tonum))(move(from)));

    throw TypeError(ty, to);
  }

  // Num => Str: same as `tostr`
  // [a] => Str: same as `join::` (still not sure I like it, but it is very helpful)
  template <> unique_ptr<Str> coerse<Str>(unique_ptr<Val> from, Type const& to) {
    Type const& ty = from->type();
    // if (app.is_strict_type() && to != ty) throw TypeError(ty, to);
    if (Ty::STR == ty.base()) return val_cast<Str>(move(from));

    if (Ty::NUM == ty.base())
      return val_cast<Str>((*static_lookup_name(tostr))(move(from)));
    if (Ty::LST == ty.base())
      return val_cast<Str>((*val_cast<Fun>((*static_lookup_name(join))(make_unique<StrChunks>(""))))(move(from)));

    throw TypeError(ty, to);
  }

  // Str => [Num]: same as `codepoints`
  // Str => [Str]: same as `graphems` - this is also fallback for Str => [a]
  // [a..] => [b..]
  // (a..) => (b..)
  // (a..) => [b..]
  // [a..] => (b..)
  template <> unique_ptr<Lst> coerse<Lst>(unique_ptr<Val> from, Type const& to) {
    Type const& ty = from->type();
    // if (app.is_strict_type() && to != ty) throw TypeError(ty, to);

    auto const& to_has = to.has();
    auto const to_size = to_has.size();

    if (Ty::LST == ty.base()) {
      if (ty == to) return val_cast<Lst>(move(from));

      // vector<Type*> const& ty_has = ty.has();
      // auto const ty_size = ty_has.size();

      // TODO: the actual conditions are bit more invloved, probly like:
      //  - list to list may be always ok
      //  - tuple to tuple ok when `to_size <= ty_size` -- (Str, Num) => (Str, Num, Str)
      //  - tuple to list ok when `to_size <= ty_size` -- (Num, Num) => [Str]
      //  - list to tuple ok when `to_size <= ty_size` -- [Num] => (Str, Num, Str)
      // .. but what happens if falls short?
      // if (ty_has.size() != size) {
      //   ostringstream oss;
      //   throw TypeError((oss << "content arity of " << ty << " does not match with " << to, oss.str()));
      // }

      return make_unique<LstMapCoerse>(val_cast<Lst>(move(from)), to_has);
    }

    if (Ty::STR == ty.base() && 1 == to_size) {
      if (Ty::NUM == to_has[0].base())
        return val_cast<Lst>((*static_lookup_name(codepoints))(move(from)));
      if (Ty::STR == to_has[0].base() || Ty::UNK == to_has[0].base())
        return val_cast<Lst>((*static_lookup_name(graphemes))(move(from)));
    }

    throw TypeError(ty, to);
  }

  // special case for type checking, (effectively a cast)
  // where `to` should be `tyor idk
  template <> unique_ptr<Fun> coerse<Fun>(unique_ptr<Val> from, Type const& to) {
    Type const& ty = from->type();
    // if (app.is_strict_type() && to != ty) throw TypeError(ty, to);
    if (Ty::FUN == to.base()) {
      if (ty.arity() != to.arity()) {
        ostringstream oss;
        throw TypeError((oss << "function arity of " << ty << " does not match with " << to, oss.str()));
      }

      return val_cast<Fun>(move(from));
    }

    ostringstream oss;
    throw TypeError((oss << "value of type " << ty << " is not a function", oss.str()));
  }

  // dispatches to the correct one dynamically
  // coersing to unk is used in builtins (eg. `const` or `id`)
  template <> unique_ptr<Val> coerse<Val>(unique_ptr<Val> from, Type const& to) {
    // Type const& ty = from->type();
    // if (app.is_strict_type() && to != ty) throw TypeError(ty, to);
    switch (to.base()) {
      case Ty::UNK: return from;
      case Ty::NUM: return coerse<Num>(move(from), to);
      case Ty::STR: return coerse<Str>(move(from), to);
      case Ty::LST: return coerse<Lst>(move(from), to);
      case Ty::FUN: return coerse<Fun>(move(from), to);
    }
    throw TypeError("miss-initialized or corrupted type");
  }


  Str_streambuf::int_type Str_streambuf::overflow(int_type) {
    return traits_type::eof();
  }

  Str_streambuf::int_type Str_streambuf::underflow() {
    // should never happend (according to some random standard)
    if (gptr() < egptr()) return *gptr();

    if (v->end()) return traits_type::eof();

    ostringstream oss;
    oss << *v;
    buffered = oss.str();

    char_type* cstr = (char_type*)(buffered.c_str());
    // why you no take const?
    setg(cstr, cstr, cstr+buffered.length());

    return buffered[0];
  }

} // namespace sel
