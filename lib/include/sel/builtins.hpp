#ifndef SEL_BUILTINS_HPP
#define SEL_BUILTINS_HPP

#include <sstream>

#include "sel/utils.hpp"
#include "sel/engine.hpp"
#include "sel/_bin_helpers.tmp.hpp"

namespace sel {

  /**
   * Seach for a value by name, return nullptr if not found.
   */
  Val* lookup_name(std::string const& name);

  namespace bin_val_helpers {

    /*
      vat: 'Val abstract type', so ex the class `Num` for Ty::NUM

      ->: Next
      <-: Base

      Head <-> Body <..> Body <-> Tail

      Head: empty struct
      Body and Tail: struct with base and arg fields

      Head and Body: extend from Fun
      Tail: extends from the last vat

      `the`, for `bin_val`, is the base class for the actual implementation of the Tail
      (for example with "add", `the` extends `Num`) because it is available at any level
      of the chain, that:
      - `T::the::args` is the arity of the function (so 2 for "add")
      - `T::the::Head` it the Head type (so `F` such as `F()(some_num)(other_num)`)
      - `T::the::Base::Next` is the actual implementation (so `Add` for "add")
      - and so `T::the::Base::Next::name` is the name constexpr ("add")
    */

    template <typename Implementation, typename... types> struct bin_val;

    // template <typename Impl, typename one>
    // struct bin_val<Impl, one> {
    //   struct Base { typedef Impl Next; }; // YYY: for `the::Base::Next` trick...
    //   struct the : bin_vat<one>::vat {
    //     the()
    //       : bin_vat<one>::vat() // ZZZ: wrong
    //     { }
    //   };
    // };

    template <typename NextT, typename to, typename from, typename from_again, typename... from_more>
    struct bin_val<NextT, to, from, from_again, from_more...> : fun<from, to>::template now_known<from_again> {
      typedef NextT Next;
      typedef bin_val<bin_val, fun<from, to>, from_again, from_more...> Base;
      typedef typename Base::the the;
      constexpr static unsigned args = Base::args + 1;
      typedef from next_arg_ty; // YYY: may remove in favor of from_again
      Base* base;
      typename Base::next_arg_ty::vat* arg;
      // this is the ctor for body types
      bin_val(Base* base, typename Base::next_arg_ty::vat* arg)
        : fun<from, to>::template now_known<typename Base::next_arg_ty>(base->type(), arg->type())
        , base(base)
        , arg(arg)
      { TRACE(bin_val<body>
          , "<arg_ty>: " << Base::next_arg_ty::make()
          , "<from>:   " << from::make()
          , "<to>:     " << to::make()
          ); }
      Val* operator()(Val* arg) override { return new Next(this, coerse<typename next_arg_ty::vat>(arg)); }
      void accept(Visitor& v) const override; // visitBody
    };

    template <typename other> struct _fun_last_ret_type { typedef other the; };
    template <typename from, typename to> struct _fun_last_ret_type<fun<from, to>> { typedef typename _fun_last_ret_type<to>::the the; };

    /**
     * typedef Head Base; // when unary,
     * typedef typename Head::Next Base; // when binary,
     * typedef typename Head::Next::Next Base; // when ternary, ... (recursive)
     */
    template <typename T> struct _one_to_nextmost;
    template <typename N, typename... tys>
    struct _one_to_nextmost<bin_val<N, tys...>> { typedef bin_val<N, tys...> the; };
    template <typename N, typename... tys1, typename... tys2>
    struct _one_to_nextmost<bin_val<bin_val<N, tys2...>, tys1...>> { typedef bin_val<N, tys2...> the; };
    template <typename N, typename... tys1, typename... tys2, typename... tys3>
    struct _one_to_nextmost<bin_val<bin_val<bin_val<N, tys3...>, tys2...>, tys1...>> { typedef typename _one_to_nextmost<bin_val<bin_val<N, tys3...>, tys2...>>::the the; };

    template <typename NextT, typename last_to, typename last_from>
    struct bin_val<NextT, last_to, last_from> : fun<last_from, last_to>::ctor {
      typedef NextT Next;
      // this is the parent class for the tail type
      struct the : _fun_last_ret_type<last_to>::the::template now_known<typename _one_to_nextmost<bin_val>::the::next_arg_ty> {
        typedef bin_val Head;
        typedef typename _one_to_nextmost<Head>::the Base;
        constexpr static unsigned args = Base::args + 1;
        Base* base;
        typename Base::next_arg_ty::vat* arg;
        // this is the ctor for the tail type
        the(Base* base, typename Base::next_arg_ty::vat* arg)
          : _fun_last_ret_type<last_to>::the::template now_known<typename Base::next_arg_ty>(base->type(), arg->type())
          , base(base)
          , arg(arg)
        { TRACE(bin_val<tail>
            , "<arg_ty>:             " << Base::next_arg_ty::make()
            , "<_fun_last_ret_type>: " << _fun_last_ret_type<last_to>::the::make()
            ); }
        void accept(Visitor& v) const override; // visitTail
      };
      // head struct does not have `.arg`
      constexpr static unsigned args = 0;
      typedef last_from next_arg_ty;
      // this is the ctor for the head type
      bin_val()
        : fun<last_from, last_to>::ctor()
      { TRACE(bin_val<head>
          , "<last_from>: " << last_from::make()
          , "<last_to>:   " << last_to::make()
          ); }
      Val* operator()(Val* arg) override { return new Next(this, coerse<typename next_arg_ty::vat>(arg)); }
      void accept(Visitor& v) const override; // visitHead
    };

    // TODO: template<>bin_val for when the last return type is of unknown
    //       (for example "flip" ends on a `Fun`...)

  } // namespace bin_val_helpers

  namespace bin {

    using bin_val_helpers::unk;
    using bin_val_helpers::num;
    using bin_val_helpers::str;
    using bin_val_helpers::lst;
    using bin_val_helpers::fun;

    using bin_val_helpers::bin_val;

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

    // REM: XXX: backward: Num <- Num <- Num
    struct Add : bin_val<Add, num, num, num>::the {
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

    struct Map : bin_val<Map, lst<unk<'b'>>, lst<unk<'a'>>, fun<unk<'a'>, unk<'b'>>>::the {
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

    struct Repeat : bin_val<Repeat, lst<unk<'a'>>, unk<'a'>>::the {
      constexpr static char const* name = "repeat";
      using the::the;
      Val* operator*() override { return nullptr; }
      Lst& operator++() override { return *this; }
      bool end() const override { return true; }
      void rewind() override { }
      size_t count() override { return 0; }
    };

    // struct Sub : bin_val<Sub, Ty::NUM, Ty::NUM, Ty::NUM>::the {
    //   constexpr static char const* name = "sub";
    //   double value() override {
    //     return ((Num*)base->arg)->value() - ((Num*)arg)->value();
    //   }
    // };

    struct Tonum : bin_val<Tonum, num, str>::the {
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

    struct Zipwith : bin_val<Zipwith, lst<unk<'c'>>, lst<unk<'b'>>, lst<unk<'a'>>, fun<unk<'a'>, fun<unk<'b'>, unk<'c'>>>>::the {
      constexpr static char const* name = "zipwith";
      using the::the;
      Val* operator*() override { return nullptr; }
      Lst& operator++() override { return *this; }
      bool end() const override { return true; }
      void rewind() override { }
      size_t count() override { return 0; }
    };

  } // namespace bin

  namespace bin_types {

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
    struct _make_bins_all<cons<bin_val_helpers::bin_val<Next, last_to, last_from>, TailL>> {
      typedef cons<
        bin_val_helpers::bin_val<Next, last_to, last_from>,
        typename _make_bins_all<TailL>::the
      > the;
    };
    template <>
    struct _make_bins_all<nil> {
      typedef nil the;
    };

    using namespace bin;
    // typedef cons_l<Add, Idk, Sub>::the bins;
    typedef cons_l<Add, Map, Repeat, Tonum, Zipwith>::the bins;
    // typedef cons_l<Add>::the bins;
    typedef _make_bins_all<bins>::the bins_all;

  } // namespace bin_types

} // namespace sel

#endif // SEL_BUILTINS_HPP
