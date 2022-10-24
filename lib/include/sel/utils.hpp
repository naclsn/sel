#ifndef SEL_UTILS_HPP
#define SEL_UTILS_HPP

#include <iostream>
#define TRACE(_f, _x) std::cerr << "[" #_f "] " << _x << "\n";

namespace sel {

  /**
   * For use in printing a pointer.
   */
  struct raw {
    void const* ptr;
    raw(void const* ptr): ptr(ptr) { }
  };

  std::ostream& operator<<(std::ostream& out, raw ptr);

}

#endif // SEL_UTILS_HPP
