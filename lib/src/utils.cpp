#include "sel/builtins.hpp"
#include "sel/utils.hpp"
#include "sel/visitors.hpp"

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

  std::ostream& operator<<(std::ostream& out, repr me) {
    VisRepr repr(out, {.single_line=me.single_line});
    me.val.accept(repr);
    return out;
  }

  std::ostream& operator<<(std::ostream& out, quoted q) {
    out << '"';
    std::string::size_type from = 0, to = q.str.find_first_of({'\t', '\n', '\r', '"'});
    while (std::string::npos != to) {
      out << q.str.substr(from, to-from) << '\\';
      switch (q.str.at(to)) {
        case '\t': out << 't'; break;
        case '\n': out << 'n'; break;
        case '\r': out << 'r'; break;
        case '"':  out << '"'; break;
      }
      from = to+1;
      to = q.str.find_first_of({'\t', '\n', '\r', '"'}, from);
    }
    out << q.str.substr(from) << '"';
    return out;
  }

  indentedbuf::int_type indentedbuf::overflow(int_type c) {
    if (pending) {
      for (unsigned k = 0; k < cur; k++)
        out.write(style, count);
      pending = false;
    }
    out.put(c);
    if ('\n' == c) pending = true;
    return c;
  }

}
