#ifndef SEL_BUILTINS_HPP
#define SEL_BUILTINS_HPP

#include "sel/utils.hpp"
#include "sel/engine.hpp"
#include "sel/_bin_helpers.tmp.hpp"

namespace sel {

  /**
   * Seach for a value by name, return nullptr if not found.
   */
  Val* lookup_name(std::string const& name);

  namespace bin_val_helpers {

    // struct tmp_unk { };
    // struct tmp_num { };
    // struct tmp_str { };
    // struct tmp_lst { };
    // struct tmp_fun { };

    template <typename Type> struct bin_vat { typedef Val vat; };
    template <> struct bin_vat<num> { typedef Num vat; };
    template <> struct bin_vat<str> { typedef Str vat; };
    template <typename has> struct bin_vat<lst<has>> { typedef Lst vat; };
    template <typename from, typename to> struct bin_vat<fun<from, to>> { typedef Fun vat; };

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

    template <typename NextT, typename From, typename... To>
    struct bin_val : Fun {
      typedef NextT Next;
      typedef bin_val<bin_val, To...> Base;
      typedef typename Base::the the;
      constexpr static unsigned args = Base::args + 1;
      // this is the ctor for body types
      bin_val()
        : Fun(Type(Ty::NUM, {0}, 0)) // ZZZ: wrong
      { }
      Base* base;
      Val* arg; // ZZZ: wrong
      inline void _setup(Base* base, Val* arg) {
        this->base = base;
        this->arg = arg;
      }
      void accept(Visitor& v) const override; // visitBody
      Val* operator()(Val* arg) override {
        auto* r = new Next();
        r->_setup(this, arg);
        return r;
      }
    };

    /**
     * typedef Head Base; // when unary,
     * typedef typename Head::Next Base; // when binary,
     * typedef typename Head::Next::Next Base; // when ternary, ... (recursive)
     */
    template <typename T> struct _one_to_nextmost;
    template <typename N, typename... Tys>
    struct _one_to_nextmost<bin_val<N, Tys...>> { typedef bin_val<N, Tys...> the; };
    template <typename N, typename... Tys1, typename... Tys2>
    struct _one_to_nextmost<bin_val<bin_val<N, Tys2...>, Tys1...>> { typedef bin_val<N, Tys2...> the; };
    template <typename N, typename... Tys1, typename... Tys2, typename... Tys3>
    struct _one_to_nextmost<bin_val<bin_val<bin_val<N, Tys3...>, Tys2...>, Tys1...>> { typedef typename _one_to_nextmost<bin_val<bin_val<N, Tys3...>, Tys2...>>::the the; };

    template <typename NextT, typename LastFrom, typename LastTo>
    struct bin_val<NextT, LastFrom, LastTo> : Fun {
      typedef NextT Next;
      // this is the parent class for the tail type
      struct the : bin_vat<LastTo>::vat {
        typedef bin_val Head;
        typedef typename _one_to_nextmost<Head>::the Base;
        constexpr static unsigned args = Base::args + 1;
        // this is the ctor for the tail type
        the()
          : bin_vat<LastTo>::vat() // ZZZ: wrong
        { }
        Base* base;
        Val* arg; // ZZZ: wrong
        inline void _setup(Base* base, Val* arg) { // (this needed because wasn't able to inherit ctor) but it will be needed to have at least the `Val* arg` in the ctor so it can build the proper `Type()`
          this->base = base;
          this->arg = arg;
        }
        void accept(Visitor& v) const override; // visitTail
      };
      // head struct does not have `.arg`
      constexpr static unsigned args = 0;
      // this is the ctor for the head type
      bin_val()
        : Fun(Type(Ty::NUM, {0}, 0)) // ZZZ: wrong
      { }
      Val* operator()(Val* arg) override {
        auto* r = new Next();
        r->_setup(this, arg);
        return r;
      }
      void accept(Visitor& v) const override; // visitHead
    };

  } // namespace bin_val_helpers

  namespace bin {

    // using bin_val_helpers::unk;
    using bin_val_helpers::num;
    // using bin_val_helpers::str;
    // using bin_val_helpers::lst;
    // using bin_val_helpers::fun;

    using bin_val_helpers::bin_val;

    struct Add : bin_val<Add, num, num, num>::the {
      constexpr static char const* name = "add";
      double value() override {
        return ((Num*)base->arg)->value() + ((Num*)arg)->value();
      }
    };

    // struct Idk : bin_val<Idk, Ty::NUM, Ty::NUM, Ty::NUM, Ty::NUM>::the {
    //   constexpr static char const* name = "idk";
    //   double value() override {
    //     return 0;
    //   }
    // };

    // struct Sub : bin_val<Sub, Ty::NUM, Ty::NUM, Ty::NUM>::the {
    //   constexpr static char const* name = "sub";
    //   double value() override {
    //     return ((Num*)base->arg)->value() - ((Num*)arg)->value();
    //   }
    // };

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
    template <typename Next, typename LastFrom, typename LastTo, typename TailL>
    struct _make_bins_all<cons<bin_val_helpers::bin_val<Next, LastFrom, LastTo>, TailL>> {
      typedef cons<
        bin_val_helpers::bin_val<Next, LastFrom, LastTo>,
        typename _make_bins_all<TailL>::the
      > the;
    };
    template <>
    struct _make_bins_all<nil> {
      typedef nil the;
    };

    using namespace bin;
    // typedef cons_l<Add, Idk, Sub>::the bins;
    typedef cons_l<Add>::the bins;
    typedef _make_bins_all<bins>::the bins_all;

  } // namespace bin_types

} // namespace sel

#endif // SEL_BUILTINS_HPP
