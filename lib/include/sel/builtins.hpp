#ifndef SEL_BUILTINS_HPP
#define SEL_BUILTINS_HPP

#include <sstream>
#include <vector>

#include "sel/utils.hpp"
#include "sel/engine.hpp"

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

  } // namespace ll

  namespace bins_helpers {

    using namespace ll;

    template <char c> struct unk {
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
     * `the`, for `bin_val`, is the base class for the actual implementation of the Tail
     * (for example with "add", `the` extends `Num`) because it is available at any level
     * of the chain, that:
     * - `T::the::args` is the arity of the function (so 2 for "add")
     * - `T::the::Head` it the Head type (so `F` such as `F()(some_num)(other_num)`)
     * - `T::the::Base::Next` is the actual implementation (so `Add` for "add")
     * - and so `T::the::Base::Next::name` is the name constexpr ("add")
     *
     * type is expected to be one of unk/num/str/lst/fun
     * defined specialisations:
     *  - <Impl, only>                       // eg. "pi"
     *  - <Impl, fun<from, to>>              // defines `the`
     *  - <Impl, fun<from1, fun<from2, to>>> // recursion for above
     */
    template <typename Implementation, typename type> struct _bin_be;

    // TODO: in template decl above
    // template <typename Impl, typename one>
    // struct bin_val<Impl, one> {
    //   struct Base { typedef Impl Next; }; // YYY: for `the::Base::Next` trick...
    //   struct the : one::ctor {
    //     typedef the Head;
    //     the()
    //       : one::ctor()
    //     { }
    //   };
    // };

    template <typename other> struct _fun_last_ret_type { typedef other the; };
    template <typename from, typename to> struct _fun_last_ret_type<fun<from, to>> { typedef typename _fun_last_ret_type<to>::the the; };

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

      struct the : _fun_last_ret_type<last_to>::the::ctor {
        typedef _bin_be Head; // type instanciated in looking up by name
        typedef typename _one_to_nextmost<Head>::the Base; // type which instantiates `this`

        typedef typename Base::_next_arg_ty Arg;

        constexpr static unsigned args = Base::args + 1;

        Base* base;
        Arg* arg;

        // this is the (inherited) ctor for the tail type
        the(Base* base, Arg* arg)
          : _fun_last_ret_type<last_to>::the::ctor(base->type(), arg->type())
          , base(base)
          , arg(arg)
        { }
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

  } // namespace bins_helpers

  /**
   * namespace containing the actual buildins
   */
  namespace bins {

    using bins_helpers::unk;
    using bins_helpers::num;
    using bins_helpers::str;
    using bins_helpers::lst;
    using bins_helpers::fun;

    using bins_helpers::_bin_be; // TODO: front-end

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
#define _bind_one(__name, __depth) auto& __name = *_depth(__depth)
#define bind_args(...) _bind_count(__VA_COUNT(__VA_ARGS__), __VA_ARGS__)

    struct Add : _bin_be<Add, ll::cons_l<num, num, num>::the>::the {
      constexpr static char const* name = "add";
      using the::the;
      double value() override {
        bind_args(a, b);
        return a.value() + b.value();
      }
    };

    // struct Idk : bin_val<Idk, Ty::NUM, Ty::NUM, Ty::NUM, Ty::NUM>::the {
    //   constexpr static char const* name = "idk";
    //   double value() override {
    //     return 0;
    //   }
    // };

    // map :: (a -> b) -> [a] -> [b] (REM: XXX: still backward)
    struct Map : _bin_be<Map, ll::cons_l<lst<unk<'b'>>, lst<unk<'a'>>, fun<unk<'a'>, unk<'b'>>>::the>::the {
      constexpr static char const* name = "map";
      using the::the;
      Val* operator*() override { // ZZZ: place holder
        bind_args(f, l);
        return f(*l);
      }
      Lst& operator++() override { return *this; }
      bool end() const override { return true; }
      void rewind() override { }
      size_t count() override { return 0; }
    };

    // struct Repeat : bin_val<Repeat, lst<unk<'a'>>, unk<'a'>>::the {
    //   constexpr static char const* name = "repeat";
    //   using the::the;
    //   Val* operator*() override { return nullptr; }
    //   Lst& operator++() override { return *this; }
    //   bool end() const override { return true; }
    //   void rewind() override { }
    //   size_t count() override { return 0; }
    // };

    // // struct Sub : bin_val<Sub, Ty::NUM, Ty::NUM, Ty::NUM>::the {
    // //   constexpr static char const* name = "sub";
    // //   double value() override {
    // //     return ((Num*)base->arg)->value() - ((Num*)arg)->value();
    // //   }
    // // };

    struct Tonum : _bin_be<Tonum, ll::cons_l<num, str>::the>::the {
      constexpr static char const* name = "tonum";
      using the::the;
      double value() override {
        bind_args(s);
        double r;
        std::stringstream ss;
        s.entire(ss);
        ss >> r;
        return r;
      }
    };

    // struct Zipwith : bin_val<Zipwith, lst<unk<'c'>>, lst<unk<'b'>>, lst<unk<'a'>>, fun<unk<'a'>, fun<unk<'b'>, unk<'c'>>>>::the {
    //   constexpr static char const* name = "zipwith";
    //   using the::the;
    //   Val* operator*() override { return nullptr; }
    //   Lst& operator++() override { return *this; }
    //   bool end() const override { return true; }
    //   void rewind() override { }
    //   size_t count() override { return 0; }
    // };

  } // namespace bin

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

    // typedef cons_l<Add, Map, Repeat, Tonum, Zipwith>::the bins;
    typedef cons_l<Add, Map, Tonum>::the bins;
    typedef _make_bins_all<bins>::the bins_all;

  } // namespace bin_types

} // namespace sel

#endif // SEL_BUILTINS_HPP
