#ifndef SEL_UTILS_HPP
#define SEL_UTILS_HPP

#include <iostream>

#define __A11(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, ...) a11
#define __VA_COUNT(...) __A11(dum, ## __VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

// #define TRACE(...)
#ifndef TRACE
// @thx: https://renenyffenegger.ch/notes/development/languages/C-C-plus-plus/preprocessor/macros/__VA_ARGS__/count-arguments
#define __CRAP2(n) __l ## n
#define __CRAP(n) __CRAP2(n)

#define __single(w) w
#define __one(o) "\n|\t" << __single(o)
#define __last(z) "\nL_\t" << __single(z)

#define __l1(a)                      __single(a)
#define __l2(a, b)                   __one(a) << __last(b)
#define __l3(a, b, c)                __one(a) << __one(b) << __last(c)
#define __l4(a, b, c, d)             __one(a) << __one(b) << __one(c) << __last(d)
#define __l5(a, b, c, d, e)          __one(a) << __one(b) << __one(c) << __one(d) << __last(e)
#define __l6(a, b, c, d, e, f)       __one(a) << __one(b) << __one(c) << __one(d) << __one(e) << __last(f)
#define __l7(a, b, c, d, e, f, g)    __one(a) << __one(b) << __one(c) << __one(d) << __one(e) << __one(f) << __last(g)
#define __l8(a, b, c, d, e, f, g, h) __one(a) << __one(b) << __one(c) << __one(d) << __one(e) << __one(f) << __one(g) << __last(h)

#define TRACE(_f, ...) std::cerr << "\e[34m[" #_f "]\e[m " << __CRAP(__VA_COUNT(__VA_ARGS__))(__VA_ARGS__) << "\n"
#endif
// #define TRACE(...)

namespace sel {

  /**
   * For use in printing a pointer.
   */
  struct raw {
    void const* ptr;
    raw(void const* ptr): ptr(ptr) { }
  };

  std::ostream& operator<<(std::ostream& out, raw ptr);


  // template <Ty Type> struct abstract_val_for { typedef Val base; };
  // template <> struct abstract_val_for<Ty::NUM> { typedef Num base; };
  // template <> struct abstract_val_for<Ty::STR> { typedef Str base; };
  // template <> struct abstract_val_for<Ty::LST> { typedef Lst base; };
  // template <> struct abstract_val_for<Ty::FUN> { typedef Fun base; };

  // template <typename Tail, typename Base, Ty From, Ty... To>
  // struct prelude_value_rec : Fun {
  //   typedef prelude_value_rec<Tail, Base, To...> next;
  //   typedef typename next::last last;
  //   prelude_value_rec(Base* base, Val* arg)
  //     : Fun(Type())
  //   { }
  //   Val* operator()(Val* arg) override { return new next(this, arg); }
  // };
  // template <typename Tail, typename Base, Ty LastFrom, Ty LastTo>
  // struct prelude_value_rec<Tail, Base, LastFrom, LastTo> : Fun {
  //   struct last : abstract_val_for<LastTo>::base {
  //     typedef prelude_value_rec base;
  //     last(prelude_value_rec* base, Val* arg)
  //       : abstract_val_for<LastTo>::base()
  //     { }
  //   };
  //   prelude_value_rec(Base* base, Val* arg)
  //     : Fun(Type())
  //   { }
  //   Val* operator()(Val* arg) override { return new Tail(this, arg); }
  // };

  // template <typename Tail, Ty From, Ty... To>
  // struct prelude_value : Fun {
  //   typedef prelude_value_rec<Tail, prelude_value, To...> next;
  //   typedef typename next::last last;
  //   prelude_value()
  //     : Fun(Type())
  //   { }
  //   Val* operator()(Val* arg) override { return new next(this, arg); }
  // };
  // // template <Ty Type>
  // // struct prelude_value<Type> : abstract_val_for<Type>::base {
  // //   prelude_value<Type>()
  // //     : abstract_val_for<Type>::base(noenv)
  // //   { }
  // // };

  // // TODO: try inside-out
  // struct AddTail;
  // struct Add : prelude_value<AddTail, Ty::NUM, Ty::NUM, Ty::NUM> { };
  // struct AddTail : Add::last {
  //   AddTail(Add::last::base* base, Val* arg): Add::last(base, arg) { }
  //   double value() override;
  // };

  // void bidoof() {
  //   Add add2 = Add();
  //   auto& add1 = *(Add::next*)add2(nullptr);
  //   Num& add0 = *(Num*)add1(nullptr);
  // }

  // // struct Pi : prelude_value<Ty::NUM> {
  // //   double value() override;
  // // };

  // // static Pi pi = Pi();

}

#endif // SEL_UTILS_HPP
