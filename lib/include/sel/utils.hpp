#ifndef SEL_UTILS_HPP
#define SEL_UTILS_HPP

#include <iostream>

#include "engine.hpp"

// @thx: https://renenyffenegger.ch/notes/development/languages/C-C-plus-plus/preprocessor/macros/__VA_ARGS__/count-arguments
#define __A11(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, ...) a11
#define __VA_COUNT(...) __A11(dum, ## __VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

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
    bool do_col;
    bool put_quo;
    inline quoted(std::string const& str): str(str), do_col(false), put_quo(true) { }
    inline quoted(std::string const& str, bool do_col, bool put_quo): str(str), do_col(do_col), put_quo(put_quo) { }
  };
  std::ostream& operator<<(std::ostream& out, quoted q);

}

#endif // SEL_UTILS_HPP
