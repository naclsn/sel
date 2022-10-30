#ifndef BIDOOF
#define BIDOOF

#if 0
map :: (a -> b) -> [a]* -> [b]*
	a -> b  ==  Str -> Num  -- a==Str -> b==Num
	[a]* -> [b]*  -- ...

Type(Ty::FUN,
  {.box_pair={
    new Type(Ty::FUN,
      {.box_pair={
        new Type(Ty::UNK, {.name=new std::string("a")}, 0),
        new Type(Ty::UNK, {.name=new std::string("b")}, 0)
      }}, 0
    ),
    new Type(Ty::FUN,
      {.box_pair={
        new Type(Ty::LST,
          {.box_has=
            types1(new Type(Ty::UNK, {.name=new std::string("a")}, 0))
          }, TyFlag::IS_INF
        ),
        new Type(Ty::LST,
          {.box_has=
            types1(new Type(Ty::UNK, {.name=new std::string("b")}, 0))
          }, TyFlag::IS_INF
        )
      }}, 0
    )
  }}, 0
)

Type(Ty::FUN,
  {.box_pair={
    new Type(Ty::LST,
      {.box_has=
        types1(new Type(arg->type().from()))
      }, TyFlag::IS_INF
    ),
    new Type(Ty::LST,
      {.box_has=
        types1(new Type(arg->type().to()))
      }, TyFlag::IS_INF
    )
  }}, 0
)
---


unk_make_lookup :: with_unk -> known -> [(name, type)]


template <with_unk, char name>
struct _extract_type_for_name<with_unk, name> {
  Type the(Val* arg) {
    return ...;
  }
};
#endif

#include "sel/types.hpp"
#include "sel/engine.hpp"

// using namespace sel;
namespace sel {
namespace bin_val_helpers {

template <char c> struct unk { inline static Type make() {
  return Type(Ty::UNK, {.name=new std::string(1, c)}, 0);
} };
struct num { inline static Type make() {
  return Type(Ty::NUM, {0}, 0);
} };
/*template <TyFlag is_inf> */struct str { inline static Type make() {
  return Type(Ty::STR, {0}, TyFlag::IS_FIN/*is_inf*/);
} };
template <typename/*...*/ has/*, TyFlag is_inf*/> struct lst { inline static Type make() {
  return Type(Ty::LST, {.box_has=types1(new Type(has::make()/*...*/))}, TyFlag::IS_FIN/*is_inf*/);
} };
template <typename from, typename to> struct fun { inline static Type make() {
  return Type(Ty::FUN, {.box_pair={new Type(from::make()), new Type(to::make())}}, 0);
} };


/**
 * for example:
 *   c == 'a'
 *   has_unknowns is "Num -> a"
 *
 * this will be a structure with an inline static function
 * `Type find(Type ty)` that return the path to the nested
 * that has the unknown named `c`
 *
 * `_<'a', "Num -> [a]">::find(ty) == ty.to().has()[0]`
 *
 * note: for function types, will prioritise `from`, ie.
 * with "a -> a" it will be `ty.from()`
 * (which should `== ty.to()` anyways)
 */
template <char c, typename has_unknowns>
struct _find_unknown {        // XXX v--- not quite sure about things, should do eg. for Num?
  inline static Type find(Type ty) { return Type(Ty::UNK, {.name=new std::string(1, c)}, 0); }
  constexpr static bool matches = false;
};

template <char c>
struct _find_unknown<c, unk<c>> {
  inline static Type find(Type ty) { return ty; }
  constexpr static bool matches = true;
};

template <char c, typename has>
struct _find_unknown<c, lst<has>> {
  inline static Type find(Type ty) {
    return _find_unknown<c, has>::find(ty.has()[0]);
  }
  constexpr static bool matches = _find_unknown<c, has>::matches;
};

template <bool is_in_from> inline Type _find_unknown_fun_get(Type ty) { return ty.from(); }
template <>                inline Type _find_unknown_fun_get<false>(Type ty) { return ty.to(); }

template <char c, typename from, typename to>
struct _find_unknown<c, fun<from, to>> {
  inline static Type find(Type ty) {
    return std::conditional<
        _find_unknown<c, from>::matches,
        _find_unknown<c, from>,
        _find_unknown<c, to>
      >::find(_find_unknown_fun_get<_find_unknown<c, from>::matches>(ty));
  }
  constexpr static bool matches
    =  _find_unknown<c, from>::matches
    || _find_unknown<c, to>::matches;
};


/**
 * for example:
 *   template_type is "[a]"
 *   has_unknowns is "a -> b"
 *
 * this will be a structure with an inline static function
 * `Type make(Type ty)` that returns `template_type` filled
 * with types found in `ty` at locations indicated by
 * `has_unknown`
 *
 * `_<"[a]", "a -> b">::make(Type("Num -> Str")) == Type("[Num]")`
 */
template <typename template_type, typename has_unknowns>
struct _now_known {
  inline static Type make(Type _) {
    return template_type::make();
  }
};

template <char c, typename hu>
struct _now_known<unk<c>, hu> {
  inline static Type make(Type ty) {
    return Type(_find_unknown<c, hu>::find(ty));
  }
};

template <typename has, typename hu>
struct _now_known<lst<has>, hu> { // hu<has<?>> -> [has<?>] -- (eg. "a -> [a]")
  inline static Type make(Type ty) {
    return Type(Ty::LST,
      {.box_has=types1(
        new Type(_now_known<has, hu>::make(ty))
      )}, 0
    );
  }
};

template <typename from, typename to, typename hu>
struct _now_known<fun<from, to>, hu> {
  inline static Type make(Type ty) {
    return Type(Ty::FUN,
      {.box_pair={
        new Type(_now_known<from, hu>::make(ty)),
        new Type(_now_known<to, hu>::make(ty))
      }}, 0
    );
  }
};

} // namespace bin_val_helpers
} // namespace sel

#endif // BIDOOF
