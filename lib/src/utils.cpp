#include "sel/utils.hpp"

namespace sel {

  std::ostream& operator<<(std::ostream& out, raw ptr) {
    ssize_t at = (ssize_t)ptr.ptr;

    int on_stack;
    int* stack = &on_stack;
    int* heap = new int;

    ssize_t to_stack = std::abs((ssize_t)stack - at);
    ssize_t to_heap = std::abs((ssize_t)heap - at);

    delete heap;
    char buf[16];
    std::sprintf(buf, "0x%lX", at);

    if (to_stack < to_heap)
      return out << "\e[35m" << buf << "\e[m";
    else
      return out << "\e[33m" << buf << "\e[m";
  }

}
