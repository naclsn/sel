#ifndef SEL_UTILS_HPP
#define SEL_UTILS_HPP

#include <iostream>

#include "engine.hpp"

#define __A11(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, ...) a11
#define __VA_COUNT(...) __A11(dum, ## __VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define TRACE(...)
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

// #define PASS
#ifndef PASS
#define PASS(_x) ((std::cerr << "\e[36m[" #_x "]\e[m " << (_x) << "\n"), _x)
#endif

namespace sel {

  /**
   * For use in printing a pointer.
   */
  struct raw {
    void const* ptr;
    raw(void const* ptr): ptr(ptr) { }
  };
  std::ostream& operator<<(std::ostream& out, raw ptr);

  /**
   * For use in representing a value.
   */
  struct repr {
    Val const& val;
    inline repr(Val const& val): val(val) { }
  };
  std::ostream& operator<<(std::ostream& out, repr me);

  /**
   * For use in representing text.
   */
  struct quoted {
    std::string const& str;
    inline quoted(std::string const& str): str(str) { }
  };
  std::ostream& operator<<(std::ostream& out, quoted q);

}

#endif // SEL_UTILS_HPP
