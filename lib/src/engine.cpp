#include <iostream>

#define TRACE(...)
#include "sel/errors.hpp"
#include "sel/engine.hpp"
#include "sel/utils.hpp"
#include "sel/builtins.hpp"
#include "sel/parser.hpp"

namespace sel {

  Val::Val(App& app, Type const& ty)
    : app(app)
    , ty(Type(ty))
  { app.push_back(this); }
  Val::~Val() { }

  void Val::accept(Visitor& v) const {
    // throw NIYError(std::string("'accept' of visitor pattern for this class: ") + typeid(*this).name());
    throw NIYError("visitor pattern not supported at all on this value");
  }

  template <typename To>
  To* coerse(App& app, Val* from, Type const& to);
  // forward
  template <> Val* coerse<Val>(App& app, Val* from, Type const& to);

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
  // [has] (recursively)
  template <> Lst* coerse<Lst>(App& app, Val* from, Type const& to) {
    TRACE(coerse<Lst>
      , "from: " << from->type()
      , "to: " << to
      );
    Type const& ty = from->type();
    if (app.is_strict_type() && to != ty) throw TypeError(ty, to);
    Type const& toto = *to.has()[0];

    if (Ty::LST == ty.base) {
      if (Ty::NUM == toto.base)
        return new LstMapCoerse<Num>(app, (Lst*)from, toto);
      if (Ty::STR == toto.base)
        return new LstMapCoerse<Str>(app, (Lst*)from, toto);
      if (Ty::LST == toto.base)
        return new LstMapCoerse<Lst>(app, (Lst*)from, toto);
      if (Ty::UNK == toto.base)
        return (Lst*)from;
    }

    if (Ty::STR == ty.base) {
      if (Ty::NUM == toto.base)
        return (Lst*)(*static_lookup_name(app, codepoints))(from);
      if (Ty::STR == toto.base || Ty::UNK == toto.base)
        return (Lst*)(*static_lookup_name(app, graphemes))(from);
    }

    //if (Ty::UNK == ty.base) // ?
    //  return (Lst*)from;

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
        throw TypeError((oss << "arity of " << ty << " does not match with " << to, oss.str()));
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

  template <typename ToHas>
  void LstMapCoerse<ToHas>::accept(Visitor& v) const {
    v.visitLstMapCoerse(type(), *this->v);
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
