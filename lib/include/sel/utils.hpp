#ifndef SEL_UTILS_HPP
#define SEL_UTILS_HPP

#include "sel/engine.hpp"
#include "sel/types.hpp"
#include <iostream>
// #define TRACE(_f, _x) std::cerr << "[" #_f "] " << _x << "\n";
#define TRACE(...)

namespace sel {

  /**
   * For use in printing a pointer.
   */
  struct raw {
    void const* ptr;
    raw(void const* ptr): ptr(ptr) { }
  };

  std::ostream& operator<<(std::ostream& out, raw ptr);


  template <Ty Type> struct abstract_val_for { typedef Val base; };
  template <> struct abstract_val_for<Ty::NUM> { typedef Num base; };
  template <> struct abstract_val_for<Ty::STR> { typedef Str base; };
  template <> struct abstract_val_for<Ty::LST> { typedef Lst base; };
  template <> struct abstract_val_for<Ty::FUN> { typedef Fun base; };

  static Env noenv;

  template <typename Tail, typename Base, Ty From, Ty... To>
  struct prelude_value_rec : Fun {
    typedef prelude_value_rec<Tail, Base, To...> next;
    typedef typename next::last last;
    prelude_value_rec(Base* base, Val* arg)
      : Fun(noenv, Type())
    { }
    Val* operator()(Val* arg) override { return new next(this, arg); }
  };
  template <typename Tail, typename Base, Ty LastFrom, Ty LastTo>
  struct prelude_value_rec<Tail, Base, LastFrom, LastTo> : Fun {
    struct last : abstract_val_for<LastTo>::base {
      typedef prelude_value_rec base;
      last(prelude_value_rec* base, Val* arg)
        : abstract_val_for<LastTo>::base(noenv)
      { }
    };
    prelude_value_rec(Base* base, Val* arg)
      : Fun(noenv, Type())
    { }
    Val* operator()(Val* arg) override { return new Tail(this, arg); }
  };

  template <typename Tail, Ty From, Ty... To>
  struct prelude_value : Fun {
    typedef prelude_value_rec<Tail, prelude_value, To...> next;
    typedef typename next::last last;
    prelude_value()
      : Fun(noenv, Type())
    { }
    Val* operator()(Val* arg) override { return new next(this, arg); }
  };
  // template <Ty Type>
  // struct prelude_value<Type> : abstract_val_for<Type>::base {
  //   prelude_value<Type>()
  //     : abstract_val_for<Type>::base(noenv)
  //   { }
  // };

  // TODO: try inside-out
  struct AddTail;
  struct Add : prelude_value<AddTail, Ty::NUM, Ty::NUM, Ty::NUM> { };
  struct AddTail : Add::last {
    AddTail(Add::last::base* base, Val* arg): Add::last(base, arg) { }
    double value() override;
  };

  void bidoof() {
    Add add2 = Add();
    auto& add1 = *(Add::next*)add2(nullptr);
    Num& add0 = *(Num*)add1(nullptr);
  }

  // struct Pi : prelude_value<Ty::NUM> {
  //   double value() override;
  // };

  // static Pi pi = Pi();

}

#endif // SEL_UTILS_HPP
