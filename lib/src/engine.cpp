#include <iostream>

#define TRACE(...)
#include "sel/builtins.hpp"
#include "sel/engine.hpp"
#include "sel/errors.hpp"
#include "sel/parser.hpp"
#include "sel/utils.hpp"
#include "sel/visitors.hpp"

namespace sel {

  Val::Val(App& app, Type const& ty)
    : app(app)
    , ty(Type(ty))
  { app.push_back(this); }
  Val::~Val() { }

  template <typename To>
  To* coerse(App& app, Val* from, Type const& to);

  // Str => Num: same as `tonum`
  template <> Num* coerse<Num>(App& app, Val* from, Type const& to) {
    TRACE(coerse<Num>
      , "from: " << from->type()
      , "to: " << to
      );
    Type const& ty = from->type();
    if (app.is_strict_type() && to != ty) throw TypeError(ty, to);
    if (Ty::NUM == ty.base) return (Num*)from;

    if (Ty::STR == ty.base)
      return (Num*)(*static_lookup_name(app, tonum))(from);

    throw TypeError(ty, to);
  }

  // Num => Str: same as `tostr`
  // [a] => Str: same as `join::` (still not sure I like it, but it is very helpful)
  template <> Str* coerse<Str>(App& app, Val* from, Type const& to) {
    TRACE(coerse<Str>
      , "from: " << from->type()
      , "to: " << to
      );
    Type const& ty = from->type();
    if (app.is_strict_type() && to != ty) throw TypeError(ty, to);
    if (Ty::STR == ty.base) return (Str*)from;

    if (Ty::NUM == ty.base)
      return (Str*)(*static_lookup_name(app, tostr))(from);
    if (Ty::LST == ty.base)
      return (Str*)(*(Fun*)(*static_lookup_name(app, join))(new StrChunks(app, "")))(from);

    throw TypeError(ty, to);
  }

  // Str => [Num]: same as `codepoints`
  // Str => [Str]: same as `graphems` - this is also fallback for Str => [a]
  // [a..] => [b..]
  // (a..) => (b..)
  // (a..) => [b..]
  // [a..] => (b..)
  template <> Lst* coerse<Lst>(App& app, Val* from, Type const& to) {
    TRACE(coerse<Lst>
      , "from: " << from->type()
      , "to: " << to
      );
    Type const& ty = from->type();
    if (app.is_strict_type() && to != ty) throw TypeError(ty, to);

    std::vector<Type*> const& to_has = to.has();
    auto const to_size = to_has.size();

    if (Ty::LST == ty.base) {
      if (ty == to) return (Lst*)from;

      // std::vector<Type*> const& ty_has = ty.has();
      // auto const ty_size = ty_has.size();

      // TODO: the actual conditions are bit more invloved, probly like:
      //  - list to list may be always ok
      //  - tuple to tuple ok when `to_size <= ty_size` -- (Str, Num) => (Str, Num, Str)
      //  - tuple to list ok when `to_size <= ty_size` -- (Num, Num) => [Str]
      //  - list to tuple ok when `to_size <= ty_size` -- [Num] => (Str, Num, Str)
      // .. but what happens if falls short?
      // if (ty_has.size() != size) {
      //   std::ostringstream oss;
      //   throw TypeError((oss << "content arity of " << ty << " does not match with " << to, oss.str()));
      // }

      return new LstMapCoerse(app, (Lst*)from, to_has);
    }

    if (Ty::STR == ty.base && 1 == to_size) {
      if (Ty::NUM == to_has[0]->base)
        return (Lst*)(*static_lookup_name(app, codepoints))(from);
      if (Ty::STR == to_has[0]->base || Ty::UNK == to_has[0]->base)
        return (Lst*)(*static_lookup_name(app, graphemes))(from);
    }

    throw TypeError(ty, to);
  }

  // special case for type checking, (effectively a cast)
  // where `to` should be `tyor idk
  template <> Fun* coerse<Fun>(App& app, Val* from, Type const& to) {
    TRACE(coerse<Fun>
      , "from: " << from->type()
      , "to: " << to
      );
    Type const& ty = from->type();
    if (app.is_strict_type() && to != ty) throw TypeError(ty, to);
    if (Ty::FUN == to.base) {

      // performs as much as arity check (get return type
      // until one is no longer Fun, if the other is still
      // Fun then it does not check out)
      // YYY: the pointer is to not invoque `operator=`
      Type const* cy = &ty; bool still_y = false;
      Type const* co = &to; bool still_o = false;
      do {
        cy = &cy->to(); still_y = Ty::FUN == cy->base;
        co = &co->to(); still_o = Ty::FUN == co->base;
      } while (still_y && still_o);

      if (still_y || still_o) {
        std::ostringstream oss;
        throw TypeError((oss << "function arity of " << ty << " does not match with " << to, oss.str()));
      }

      return (Fun*)from;
    }

    std::ostringstream oss;
    throw TypeError((oss << "value of type " << ty << " is not a function", oss.str()));
  }

  // dispatches to the correct one dynamically
  // coersing to unk is used in builtins (eg. `const` or `id`)
  template <> Val* coerse<Val>(App& app, Val* from, Type const& to) {
    TRACE(coerse<Val>
      , "from: " << from->type()
      , "to: " << to
      );
    Type const& ty = from->type();
    if (app.is_strict_type() && to != ty) throw TypeError(ty, to);
    switch (to.base) {
      case Ty::UNK: return from;
      case Ty::NUM: return coerse<Num>(app, from, to);
      case Ty::STR: return coerse<Str>(app, from, to);
      case Ty::LST: return coerse<Lst>(app, from, to);
      case Ty::FUN: return coerse<Fun>(app, from, to);
    }
    throw TypeError("miss-initialized or corrupted type");
  }


  Str_streambuf& Str_streambuf::operator=(Str_streambuf&& sis) {
    if (this == &sis) return *this;
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
