#ifndef SEL_BUILTINS_HPP
#define SEL_BUILTINS_HPP

#include <sstream>
#include <vector>

#include "utils.hpp"
#include "engine.hpp"
#include "errors.hpp"

namespace sel {

  class StrChunks : public Str {
    typedef std::vector<std::string> ch_t;
    ch_t chunks;
    ch_t::size_type at;
  public:
    StrChunks(std::vector<std::string> chunks)
      : Str(TyFlag::IS_FIN)
      , chunks(chunks)
      , at(0)
    { }
    StrChunks(std::string single)
      : StrChunks(std::vector<std::string>({single}))
    { }
    std::ostream& stream(std::ostream& out) override {
      return out << chunks[at++];
    }
    bool end() const override {
      return chunks.size() <= at;
    }
    void rewind() override {
      at = 0;
    }
    std::ostream& entire(std::ostream& out) override {
      for (auto const& it : chunks)
        out << it;
      return out;
    }
  };

  /**
   * Seach for a value by name, return nullptr if not found.
   */
  Val* lookup_name(std::string const& name);

  /**
   * (mostly internal) namespace for TMP linked list
   */
  namespace ll {

    /**
     * empty list, see `ll::cons`
     */
    struct nil { };

    /**
     * linked lists of type (car,cdr) style, see `ll::nil`
     */
    template <typename A, typename D>
    struct cons { typedef A car; typedef D cdr; };
    template <typename O>
    struct cons<O, nil> { typedef O car; typedef nil cdr; };

    /**
     * make a linked lists of types (car,cdr) style from pack (see `ll::cons`)
     */
    template <typename H, typename... T>
    struct cons_l { typedef cons<H, typename cons_l<T...>::the> the; };
    template <typename O>
    struct cons_l<O> { typedef cons<O, nil> the; };

    template <typename from, typename into> struct _rev_impl;
    /**
     * reverse a list of types
     */
    template <typename list>
    struct rev { typedef typename _rev_impl<list, nil>::the the; };
    template <typename into>
    struct _rev_impl<nil, into> { typedef into the; };
    template <typename H, typename T, typename into>
    struct _rev_impl<cons<H, T>, into> { typedef typename _rev_impl<T, cons<H, into>>::the the; };

  } // namespace ll

  /**
   * namespace with types used to help constructing builtins
   */
  namespace bins_helpers {

    using namespace ll;

    template <char c> struct unk {
      typedef Val vat;
      inline static Type make() {
        // TODO: will need to grab the `::name` of the
        // function somehow so the name of the type can
        // be <char>_<function>
        return Type(Ty::UNK, {.name=new std::string(1, c)}, 0);
      }
    };
    struct num {
      typedef Num vat;
      inline static Type make() {
        return Type(Ty::NUM, {0}, 0);
      }
      struct ctor : Num {
        ctor()
          : Num()
        { }
        ctor(Type const& base_fty, Type const& ty)
          : Num()
        { }
      };
    };
    /*template <TyFlag is_inf> */struct str {
      typedef Str vat;
      inline static Type make() {
        return Type(Ty::STR, {0}, TyFlag::IS_FIN/*is_inf*/);
      }
      struct ctor : Str {
        ctor()
          : Str(TyFlag::IS_FIN) // ZZZ: from template param
        { }
        ctor(Type const& base_fty, Type const& ty)
          : Str(TyFlag::IS_FIN) // ZZZ: from base_fty.applied(ty)
        { }
      };
    };
    template <typename/*...*/ has/*, TyFlag is_inf*/> struct lst {
      typedef Lst vat;
      inline static Type make() {
        return Type(Ty::LST, {.box_has=types1(new Type(has::make()/*...*/))}, TyFlag::IS_FIN/*is_inf*/);
      }
      struct ctor : Lst {
        ctor(): Lst(make()) { }
        ctor(Type const& base_fty, Type const& ty)
          : Lst(base_fty.applied(ty))
        { }
      };
    };
    template <typename from, typename to> struct fun {
      typedef Fun vat;
      inline static Type make() {
        return Type(Ty::FUN, {.box_pair={new Type(from::make()), new Type(to::make())}}, 0);
      }
      struct ctor : Fun {
        ctor(): Fun(make()) { }
        ctor(Type const& base_fty, Type const& ty)
          : Fun(base_fty.applied(ty))
        { }
      };
    };

    /*
     * vat: 'Val abstract type', so ex the class `Num` for Ty::NUM
     *
     * ->: Next
     * <-: Base
     *
     * Head <-> Body <..> Body <-> Tail
     *
     * Head: empty struct
     * Body and Tail: struct with base and arg fields
     *
     * Head and Body: extend from Fun
     * Tail: extends from the last vat
     *
     * `the`, for `_bin_be`, is the base class for the actual implementation of the Tail
     * (for example with "add", `the` extends `Num`) because it is available at any level
     * of the chain, that:
     * - `T::the::args` is the arity of the function (so 2 for "add")
     * - `T::the::Head` it the Head type (so `F` such as `F()(some_num)(other_num)`)
     * - `T::the::Base::Next` is the actual implementation (so `Add` for "add")
     * - and so `T::the::Base::Next::name` is the name constexpr ("add")
     *
     * `type` is expected to be a ll of unk/num/str/lst/fun
     * defined specialisations:
     *  - <Impl, cons<one, nil>>                                     // eg. "pi" (note that it's not `<Impl, one>`!)
     *  - <Impl, cons<last_to, cons<last_from, nil>>>                // defines `the`
     *  - <Impl, cons<to, cons<from, cons<from_again, from_more>>>>  // recursion for above
     *
     * when the last element of `type` is of unk, the implementation in the
     * tail struct is done by overriding a method called `Val* impl()`
     *
     * not use directly, see `builtin`
     */
    template <typename Implementation, typename reversed_type> struct _bin_be;

    // TODO: in template decl above
    // template <typename Impl, typename one>
    // struct _bin_be<Impl, cons<one, nil>> {
    //   struct Base { typedef Impl Next; }; // YYY: for `the::Base::Next` trick...
    //   struct the : one::ctor {
    //     typedef the Head;
    //     the()
    //       : one::ctor()
    //     { }
    //   };
    // };

    template <typename other> struct _fun_first_par_type { typedef void the; }; // ZZZ
    template <typename from, typename to> struct _fun_first_par_type<fun<from, to>> { typedef from the; };

    template <typename other> struct _fun_last_ret_type { typedef other the; };
    template <typename from, typename to> struct _fun_last_ret_type<fun<from, to>> { typedef typename _fun_last_ret_type<to>::the the; };

    template <typename other>
    struct _one_to_fun_last_ret_type { typedef void the; };
    template <typename from, typename to>
    struct _one_to_fun_last_ret_type<fun<from, to>> { typedef fun<from, to> the; };
    template <typename from, typename to_from, typename to_to>
    struct _one_to_fun_last_ret_type<fun<from, fun<to_from, to_to>>> { typedef typename _one_to_fun_last_ret_type<fun<to_from, to_to>>::the the; };

    template <typename R> struct _is_unk { constexpr static bool the = false; };
    template <char c> struct _is_unk<unk<c>> { constexpr static bool the = true; };

    /**
     * typedef Head Base; // when unary,
     * typedef typename Head::Next Base; // when binary,
     * typedef typename Head::Next::Next Base; // when ternary, ... (recursive)
     */
    template <typename T> struct _one_to_nextmost;
    template <typename N, typename ty>
    struct _one_to_nextmost<_bin_be<N, ty>> { typedef _bin_be<N, ty> the; };
    template <typename N, typename ty1, typename ty2>
    struct _one_to_nextmost<_bin_be<_bin_be<N, ty2>, ty1>> { typedef _bin_be<N, ty2> the; };
    template <typename N, typename ty1, typename ty2, typename ty3>
    struct _one_to_nextmost<_bin_be<_bin_be<_bin_be<N, ty3>, ty2>, ty1>> { typedef typename _one_to_nextmost<_bin_be<_bin_be<N, ty3>, ty2>>::the the; };

    template <typename NextT, typename last_to, typename last_from>
    struct _bin_be<NextT, cons<last_to, cons<last_from, nil>>> : fun<last_from, last_to>::ctor {
      typedef NextT Next; // type `this` instantiates in `op()`

      typedef typename last_from::vat _next_arg_ty;

      typedef typename _fun_last_ret_type<last_to>::the _ty_tail;
      typedef typename _one_to_fun_last_ret_type<fun<last_from, last_to>>::the _ty_one_to_tail;

      constexpr static bool _is_unk_tail = _is_unk<_ty_tail>::the;

      // is not used when `_is_unk_tail` (eg. 'X(a) -> a')
      struct _the_when_not_unk : std::conditional<
          _is_unk_tail,
          num, // (YYY: anything that has `::ctor` with a fitting `virtual ::accept`)
          _ty_tail
      >::type::ctor {
        typedef _bin_be Head; // type instanciated in looking up by name
        typedef typename _one_to_nextmost<Head>::the Base; // type which instantiates `this`

        typedef typename Base::_next_arg_ty Arg;

        constexpr static unsigned args = Base::args + 1;

        Base* base;
        Arg* arg;

        // this is the (inherited) ctor for the tail type
        _the_when_not_unk(Base* base, Arg* arg)
          : _ty_tail::ctor(base->type(), arg->type())
          , base(base)
          , arg(arg)
        { }
      };

      // is used when `_is_unk_tail` (eg. 'X(a) -> a')
      struct _the_when_is_unk : _ty_one_to_tail::ctor {
        typedef _bin_be Head; // type instanciated in looking up by name
        typedef typename _one_to_nextmost<Head>::the Base; // type which instantiates `this`

        typedef typename Base::_next_arg_ty Arg;

        constexpr static unsigned args = Base::args + 1; // YYY: ?

        // tldr: inserts the arg as this own `arg`, and push back `base` once
        struct _ProxyBase : _ty_one_to_tail::ctor {
          typedef _the_when_is_unk the;
          constexpr static unsigned args = Base::args + 1;
          Base* base;
          Arg* arg;
          _ProxyBase(Base* base, Arg* arg)
            : _ty_one_to_tail::ctor(base->type(), arg->type()) // YYY: too hacky?
            , base(base)
            , arg(arg)
          { }
          Val* operator()(Val* arg) override { return nullptr; } // YYY: still quite hacky?
        } _base;
        _ProxyBase* base;
        typedef typename _fun_first_par_type<_ty_one_to_tail>::the::vat _LastArg;
        _LastArg* arg;

        // this is the (inherited) ctor for the tail type when ends on unk
        _the_when_is_unk(Base* base, Arg* arg)
          : _ty_one_to_tail::ctor(base->type(), arg->type())
          , _base(base, arg)
          , base(&_base)
          , arg(nullptr)
        { }

        // to be overriden in `Implementation`
        virtual Val* impl() = 0;

        Val* operator()(Val* arg) override {
          this->arg = coerse<_LastArg>(arg);
          return impl();
        }
      };

      typedef typename std::conditional<
          _is_unk_tail,
          _the_when_is_unk,
          _the_when_not_unk
      >::type _the;
      struct the : _the {
        typedef typename _the::Head Head;
        typedef typename _the::Base Base;
        constexpr static unsigned args = _the::args;
        using _the::_the;
        void accept(Visitor& v) const override; // visitTail
      };

      constexpr static unsigned args = 0;

      // this is the ctor for the head type
      _bin_be()
        : fun<last_from, last_to>::ctor()
      { }

      Val* operator()(Val* arg) override { return new Next(this, coerse<_next_arg_ty>(arg)); }
      void accept(Visitor& v) const override; // visitHead
    };

    template <typename NextT, typename to, typename from, typename from_again, typename from_more>
    struct _bin_be<NextT, cons<to, cons<from, cons<from_again, from_more>>>> : fun<from, to>::ctor {
      typedef NextT Next; // type `this` instantiates in `op()`
      typedef _bin_be<_bin_be, cons<fun<from, to>, cons<from_again, from_more>>> Base; // type which instantiates `this`

      typedef typename from::vat _next_arg_ty;
      typedef typename Base::_next_arg_ty Arg;

      typedef typename Base::the the; // bubble `the` up

      constexpr static unsigned args = Base::args + 1;

      Base* base;
      Arg* arg;

      // this is the ctor for body types
      _bin_be(Base* base, Arg* arg)
        : fun<from, to>::ctor(base->type(), arg->type())
        , base(base)
        , arg(arg)
      { }

      Val* operator()(Val* arg) override { return new Next(this, coerse<_next_arg_ty>(arg)); }
      void accept(Visitor& v) const override; // visitBody
    };

    template <typename list>
    struct _make_fun_if_unk_tail { typedef list the; };
    template <char c, typename head, typename tail>
    struct _make_fun_if_unk_tail<cons<unk<c>, cons<head, tail>>> { typedef cons<fun<head, unk<c>>, tail> the; };

    template <typename Implementation, typename type>
    struct builtin {
      typedef typename _bin_be<Implementation, typename _make_fun_if_unk_tail<typename rev<type>::the>::the>::the the;
    };

  } // namespace bins_helpers

  /**
   * namespace containing the actual builtins
   */
  namespace bins {

    using bins_helpers::unk;
    using bins_helpers::num;
    using bins_helpers::str;
    using bins_helpers::lst;
    using bins_helpers::fun;

#define _BIN_num \
      double value() override; \
      using ___ = void
#define _BIN_str \
      std::ostream& stream(std::ostream& out) override; \
      bool end() const override; \
      void rewind() override; \
      std::ostream& entire(std::ostream& out) override; \
      using ___ = void //___discard // TODO: (when proper handling of ty flags)
#define _BIN_lst \
      Val* operator*() override; \
      Lst& operator++() override; \
      bool end() const override; \
      void rewind() override; \
      size_t count() override; \
      using ___ = ___discard
#define _BIN_fun \
      Val* impl() override; \
      using ___ = ___discard
#define _BIN_(__ty) _BIN_ ## __ty

#define _nth_(__nth) _nth_ ## __nth
#define _nth_1(a)             a
#define _nth_2(b, a)          a
#define _nth_3(c, b, a)       a
#define _nth_4(d, c, b, a)    a
#define _nth_5(e, d, c, b, a) a
#define _nth_count(__count, ...) _nth_(__count)(__VA_ARGS__)

#define _LAST(...) _nth_count(__VA_COUNT(__VA_ARGS__), __VA_ARGS__)

#define _BIN(__ty) _BIN_(__ty)
// for types like `lst<something>`, this used to discard the `something`
template <typename... _> struct ___discard;
// used to remove the parenthesis from `__decl` and `__body`
#define _rem_par(...) __VA_ARGS__

#define BIN(__ident, __decl, __body) \
    struct __ident##_ \
        : bins_helpers::builtin<__ident##_, ll::cons_l<_rem_par __decl>::the>::the { \
      constexpr static char const* name = #__ident; \
      using the::the; \
      _BIN(_LAST __decl); \
      _rem_par __body \
    }

// YYY: C-pp cannot do case operations in stringify,
// so to still have no conflict with the 65+ kw of C++,
// strategy is `lower_` (eg. `add_`, has `::name = "add"`)
// (na, couldn't get it with a constexpr tolower either...)
#define DECL(__ident, ...) \
    __ident##_ : bins_helpers::builtin<__ident##_, ll::cons_l<__VA_ARGS__>::the>::the
// XXX: name is still repeated twice, which not cool, but
// other approaches were making code more annoying to read,
// worst for static tools (eg. sy highlight...)... (pb is:
// the name must appear both outside and inside the struct)
// SEE: https://stackoverflow.com/a/4225302
#define BODY(__ident) \
      constexpr static char const* name = #__ident; \
      using the::the;

    struct DECL(abs, num, num) { BODY(abs);
      double value() override;
    };

    struct DECL(add, num, num, num) { BODY(add);
      double value() override;
    };

    struct DECL(map, fun<unk<'a'>, unk<'b'>>, lst<unk<'a'>>, lst<unk<'b'>>) { BODY(map);
      Val* curr = nullptr;
      Val* operator*() override;
      Lst& operator++() override;
      bool end() const override;
      void rewind() override;
      size_t count() override;
    };

    struct DECL(flip, fun<unk<'a'>, fun<unk<'b'>, unk<'c'>>>, unk<'b'>, unk<'a'>, unk<'c'>) { BODY(flip);
      Val* impl() override;
    };

    struct DECL(join, str, lst<str>, str) { BODY(join);
      bool beginning = true;
      std::ostream& stream(std::ostream& out) override;
      bool end() const override;
      void rewind() override;
      std::ostream& entire(std::ostream& out) override;
    };

    struct DECL(repeat, unk<'a'>, lst<unk<'a'>>) { BODY(repeat);
      Val* operator*() override;
      Lst& operator++() override;
      bool end() const override;
      void rewind() override;
      size_t count() override;
    };

    struct DECL(split, str, str, lst<str>) { BODY(split);
      bool did_once = false;
      std::string ssep;
      std::ostringstream acc = std::ostringstream(std::ios_base::ate);
      std::string curr;
      bool at_end = false;
      // std::vector<Val*> cache;
      bool init = false;
      void once();
      void next();
      Val* operator*() override;
      Lst& operator++() override;
      bool end() const override;
      void rewind() override;
      size_t count() override;
    };

    struct DECL(sub, num, num, num) { BODY(sub);
      double value() override;
    };

    struct DECL(tonum, str, num) { BODY(tonum);
      double value() override;
    };

    struct DECL(tostr, num, str) { BODY(tostr);
      bool read = false;
      std::ostream& stream(std::ostream& out) override;
      bool end() const override;
      void rewind() override;
      std::ostream& entire(std::ostream& out) override;
    };

    struct DECL(zipwith, fun<unk<'a'>, fun<unk<'b'>, unk<'c'>>>, lst<unk<'a'>>, lst<unk<'b'>>, lst<unk<'c'>>) { BODY(zipwith);
      Val* operator*() override;
      Lst& operator++() override;
      bool end() const override;
      void rewind() override;
      size_t count() override;
    };

  } // namespace bins

  /**
   * namespace with the types `bins` and `bins_all` which lists
   *  - every builtin `Tail` types in alphabetical order
   *  - every builtin types (inc. intermediary) (not in any order)
   */
  namespace bins_ll {

    using namespace ll;

    /**
     * make a list from a list, by also unpacking every `::Base`s
     */
    template <typename TailL>
    struct _make_bins_all {
      typedef cons<
        typename TailL::car,
        typename _make_bins_all<cons<typename TailL::car::Base, typename TailL::cdr>>::the
      > the;
    };
    template <typename Next, typename last_to, typename last_from, typename TailL>
    struct _make_bins_all<cons<bins_helpers::_bin_be<Next, cons<last_to, cons<last_from, nil>>>, TailL>> {
      typedef cons<
        bins_helpers::_bin_be<Next, cons<last_to, cons<last_from, nil>>>,
        typename _make_bins_all<TailL>::the
      > the;
    };
    template <>
    struct _make_bins_all<nil> {
      typedef nil the;
    };

    using namespace bins;

    // XXX: still would love if this list could be built automatically
    typedef cons_l
      < abs_
      , add_
      , flip_
      , join_
      , map_
      , repeat_
      , split_
      , sub_
      , tonum_
      , tostr_
      , zipwith_
      >::the bins;
    typedef _make_bins_all<bins>::the bins_all;

  } // namespace bins_ll

} // namespace sel

#endif // SEL_BUILTINS_HPP
