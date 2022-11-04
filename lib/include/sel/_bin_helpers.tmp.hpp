// ZZZ
#ifndef BIDOOF
#define BIDOOF

#include "sel/types.hpp"
#include "sel/engine.hpp"
#include "sel/utils.hpp"

// using namespace sel;
namespace sel {
namespace bin_val_helpers {

template <typename template_type, typename has_unknowns>
struct _now_known; // ZZZ: forward decl

template <char c> struct unk {
  typedef Val vat;
  inline static Type make() {
    return Type(Ty::UNK, {.name=new std::string(1, c)}, 0);
  }
  typedef void ctor; // YYY: probably should never
  typedef void now_known; // YYY: probably should never
};
struct num {
  typedef Num vat;
  inline static Type make() {
    return Type(Ty::NUM, {0}, 0);
  }
  struct ctor : Num {
    ctor(Type const& base_fty, Type const& ty): Num() { }
  };
  template <typename has_unknowns>
  struct now_known : Num {
    now_known(Type const& base_fty, Type const& ty): Num() { }
  };
};
/*template <TyFlag is_inf> */struct str {
  typedef Str vat;
  inline static Type make() {
    return Type(Ty::STR, {0}, TyFlag::IS_FIN/*is_inf*/);
  }
  struct ctor : Str {
    ctor(Type const& base_fty, Type const& ty): Str(TyFlag::IS_FIN) { } // ZZZ
  };
  template <typename has_unknowns>
  struct now_known : Str {
    now_known(Type const& base_fty, Type const& ty): Str(TyFlag::IS_FIN) { } // ZZZ
  };
};
template <typename/*...*/ has/*, TyFlag is_inf*/> struct lst {
  typedef Lst vat;
  inline static Type make() {
    return Type(Ty::LST, {.box_has=types1(new Type(has::make()/*...*/))}, TyFlag::IS_FIN/*is_inf*/);
  }
  struct ctor : Lst {
    ctor(Type const& base_fty, Type const& ty): Lst(make()) {
      // TODO: ZZZ
      TRACE(lst::ctor
        , "<has>:            " << has::make()
        , "template_type:    " << base_fty.to()
        , "<template_type>:  " << make()
        , "has_unknowns:     " << base_fty.from()
        , "arg to now_known: " << ty
        //, "magic:            " << (_now_known<lst, base_fty.from()>::make(ty))
        );
    }
    ctor(): Lst(make()) { }
  };
  template <typename has_unknowns>
  struct now_known : Lst {
    now_known(Type const& base_fty, Type const& ty): Lst(_now_known<lst, has_unknowns>::make(ty)) {
      // TODO: ZZZ
      TRACE(lst::now_known
        , "<has>:            " << has::make()
        , "template_type:    " << base_fty.to()
        , "<template_type>:  " << make()
        , "has_unknowns:     " << base_fty.from()
        , "<has_unknowns>:   " << has_unknowns::make()
        , "arg to now_known: " << ty
        , "magic:            " << (_now_known<lst, has_unknowns>::make(ty))
        );
    }
  };
};
template <typename from, typename to> struct fun {
  typedef Fun vat;
  inline static Type make() {
    return Type(Ty::FUN, {.box_pair={new Type(from::make()), new Type(to::make())}}, 0);
  }
  struct ctor : Fun {
    ctor(Type const& base_fty, Type const& ty): Fun(make()) {
      // TODO: ZZZ
      TRACE(fun::ctor
        , "<from>:           " << from::make()
        , "<to>:             " << to::make()
        , "template_type:    " << base_fty.to()
        , "<template_type>:  " << make()
        , "has_unknowns:     " << base_fty.from()
        , "arg to now_known: " << ty
        //, "magic:            " << (_now_known<fun, base_fty.from()>::make(ty))
        );
    }
    ctor(): Fun(make()) { }
  };
  template <typename has_unknowns>
  struct now_known : Fun {
    now_known(Type const& base_fty, Type const& ty): Fun(_now_known<fun, has_unknowns>::make(ty)) {
      // TODO: ZZZ
      TRACE(fun::now_known
        , "<from>:           " << from::make()
        , "<to>:             " << to::make()
        , "template_type:    " << base_fty.to()
        , "<template_type>:  " << make()
        , "has_unknowns:     " << base_fty.from()
        , "<has_unknowns>:   " << has_unknowns::make()
        , "arg to now_known: " << ty
        , "magic:            " << (_now_known<fun, has_unknowns>::make(ty))
        );
    }
  };
};


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
    return _find_unknown<c, has>::find(*ty.has()[0]); // TODO: remove this * with `Type const& ty`
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
      >::type::find(_find_unknown_fun_get<_find_unknown<c, from>::matches>(ty));
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
