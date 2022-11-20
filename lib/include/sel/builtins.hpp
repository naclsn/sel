#ifndef SEL_BUILTINS_HPP
#define SEL_BUILTINS_HPP

#include <sstream>
#include <vector>

#include "utils.hpp"
#include "engine.hpp"
#include "errors.hpp"

namespace sel {

  /**
   * Seach for a value by name, return nullptr if not found.
   */
  Val* lookup_name(std::string const& name);

  /**
   * (mostly internal) namespace for TMP linked list
   */
  namespace ll {

    /**
     * empty list, see `bin_types::cons`
     */
    struct nil { };

    /**
     * linked lists of type (car,cdr) style, see `bin_types::nil`
     */
    template <typename A, typename D>
    struct cons { typedef A car; typedef D cdr; };
    template <typename O>
    struct cons<O, nil> { typedef O car; typedef nil cdr; };

    /**
     * make a linked lists of types (car,cdr) style from pack (see `bin_types::cons`)
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

  // static_assert(std::is_same<_ty_true_ret, unk<'c'>>(), "");
  // static_assert(std::is_same<_ty_oneto_true_ret, fun<unk<'a'>, unk<'c'>>>(), "");

      // typedef typename std::conditional<
      //   _is_unk<_ty_true_ret>::the,
      //   _ty_oneto_true_ret,
      //   _ty_true_ret
      // >::type _ty_tail;

  // static_assert(std::is_same<_ty_tail, fun<unk<'a'>, unk<'c'>>>(), "");

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
        // void accept(Visitor& v) const override; // visitTail
      };

      // is used when `_is_unk_tail` (eg. 'X(a) -> a')
      struct _the_when_is_unk : _ty_one_to_tail::ctor {
        typedef _bin_be Head; // type instanciated in looking up by name
        typedef typename _one_to_nextmost<Head>::the Base; // type which instantiates `this`

        typedef typename Base::_next_arg_ty Arg;

        constexpr static unsigned args = Base::args + 1; // YYY: ?

        // tldr: inserts the arg as this own `arg`, and push back `base` once
        struct _ProxyBase {
          Base* base;
          Arg* arg;
          _ProxyBase(Base* base, Arg* arg)
            : base(base)
            , arg(arg)
          { }
        } _base;
        _ProxyBase* base;
        typedef typename _fun_first_par_type<_ty_one_to_tail>::the::vat _LastArg;
        _LastArg* arg;

        // this is the (inherited) ctor for the tail type when ends on unk
        _the_when_is_unk(Base* base, Arg* arg)
          : _ty_one_to_tail::ctor(base->type(), arg->type())
          , _base(base, arg)
          , arg(nullptr)
        { }
        // void accept(Visitor& v) const override; // visitTail

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

    template <typename Implementation, typename type>
    struct builtin {
      typedef typename _bin_be<Implementation, typename rev<type>::the>::the the;
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

#define _depth(__depth) _depth_ ## __depth
#define _depth_0 arg
#define _depth_1 base->_depth_0
#define _depth_2 base->_depth_1
#define _depth_3 base->_depth_2

#define _bind_some(__count) _bind_some_ ## __count
#define _bind_some_1(a)          _bind_one(a, 0)
#define _bind_some_2(a, b)       _bind_one(a, 1); _bind_some_1(b)
#define _bind_some_3(a, b, c)    _bind_one(a, 2); _bind_some_2(b, c)
#define _bind_some_4(a, b, c, d) _bind_one(a, 3); _bind_some_3(b, c, d)

#define _bind_count(__count, ...) _bind_some(__count)(__VA_ARGS__)
#define _bind_one(__name, __depth) auto& __name = *this->_depth(__depth)
// YYY: could it somehow be moved into `BODY`? in a way
// that it is only written once and the named arg refs
// are available all throughout the struct
#define bind_args(...) _bind_count(__VA_COUNT(__VA_ARGS__), __VA_ARGS__)

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
#define BODY(__ident) \
      constexpr static char const* name = #__ident; \
      using the::the;

    struct DECL(abs, num, num) { BODY(abs);
      double value() override {
        return std::abs(arg->value());
      }
    };

    struct DECL(add, num, num, num) { BODY(add);
      double value() override {
        bind_args(a, b);
        return a.value() + b.value();
      }
    };

    struct DECL(map, fun<unk<'a'>, unk<'b'>>, lst<unk<'a'>>, lst<unk<'b'>>) { BODY(map);
      Val* operator*() override { // ZZZ: place holder
        bind_args(f, l);
        return f(*l);
      }
      Lst& operator++() override { return *this; }
      bool end() const override { return true; }
      void rewind() override { }
      size_t count() override { return 0; }
    };

    // TODO: dont know how, but would like to static_assert that this   vvv
    //       is `fun` when the decl type ends in `unk` (or add it..?)   vvv
    struct DECL(flip, fun<unk<'a'>, fun<unk<'b'>, unk<'c'>>>, unk<'b'>, fun<unk<'a'>, unk<'c'>>) { BODY(flip);
    // struct DECL(flip, fun<unk<'a'>, fun<unk<'b'>, unk<'c'>>>, unk<'b'>, unk<'a'>, unk<'c'>) { BODY(flip);
      Val* impl() override {
        bind_args(fun, b, a);
        return (*(Fun*)fun(&a))(&b);
      }
    };

    struct DECL(join, str, lst<str>, str) { BODY(join);
      bool beginning = true;
      std::ostream& stream(std::ostream& out) override {
        bind_args(sep, lst);
        if (beginning) beginning = false;
        else sep.entire(out);
        Str* it = (Str*)*lst++;
        return it->entire(out);
      }
      bool end() const override {
        Lst& lst = *arg;
        return lst.end();
      }
      void rewind() override {
        Lst& lst = *arg;
        lst.rewind();
        beginning = true;
      }
      std::ostream& entire(std::ostream& out) override {
        bind_args(sep, lst);
        if (lst.end()) return out;
        out << *lst;
        while (!lst.end()) {
          sep.rewind();
          sep.entire(out);
          out << *(Str*)*(++lst);
        }
        return out;
      }
    };

    struct DECL(repeat, unk<'a'>, lst<unk<'a'>>) { BODY(repeat);
      Val* operator*() override { return nullptr; }
      Lst& operator++() override { return *this; }
      bool end() const override { return true; }
      void rewind() override { }
      size_t count() override { return 0; }
    };

    struct DECL(split, str, str, lst<str>) { BODY(split);
      Val* operator*() override {
        throw NIYError("Val* operator*()", "- what -");
        // Str& sep = *base->arg;
        // Str& str = *arg;
        return nullptr;
      }
      Lst& operator++() override {
        throw NIYError("Lst& operator++()", "- what -");
        // Str& sep = *base->arg;
        // Str& str = *arg;
        return *this;
      }
      bool end() const override {
        Str& str = *arg;
        return str.end();
      }
      void rewind() override {
        Str& str = *arg;
        str.rewind();
      }
      size_t count() override {
        throw NIYError("size_t count()", "- what -");
      }
    };

    struct DECL(sub, num, num, num) { BODY(sub);
      double value() override {
        bind_args(a, b);
        return a.value() - b.value();
      }
    };

    struct DECL(tonum, str, num) { BODY(tonum);
      double value() override {
        bind_args(s);
        double r;
        std::stringstream ss;
        s.entire(ss);
        ss >> r;
        return r;
      }
    };

    struct DECL(tostr, num, str) { BODY(tostr);
      bool read = false;
      std::ostream& stream(std::ostream& out) override { read = true; return out << arg->value(); }
      bool end() const override { return read; }
      void rewind() override { read = false; }
      std::ostream& entire(std::ostream& out) override { read = true; return out << arg->value(); }
    };

    struct DECL(zipwith, fun<unk<'a'>, fun<unk<'b'>, unk<'c'>>>, lst<unk<'a'>>, lst<unk<'b'>>, lst<unk<'c'>>) { BODY(zipwith);
      Val* operator*() override { return nullptr; }
      Lst& operator++() override { return *this; }
      bool end() const override { return true; }
      void rewind() override { }
      size_t count() override { return 0; }
    };

  } // namespace bin

  /**
   * namespace with the types `bins` and `bins_all` which lists
   *  - every builtin `Tail` types in alphabetical order
   *  - every builtin types (inc. intermediary) (not in any order)
   */
  namespace bin_types {

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

  } // namespace bin_types

} // namespace sel

#endif // SEL_BUILTINS_HPP
