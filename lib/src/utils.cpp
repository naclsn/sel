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
    if (q.put_quo) out << (q.do_col ? ':' : '"');
    std::string const search =
      { '\a'
      , '\b'
      , '\t'
      , '\n'
      , '\v'
      , '\f'
      , '\r'
      , '\e'
      , q.do_col ? ':' : '"'
      };
    std::string::size_type from = 0, to = q.str.find_first_of(search);
    while (std::string::npos != to) {
      if (q.do_col && ':' == q.str.at(to)) {
        out << "::";
      } else {
        out << q.str.substr(from, to-from) << '\\';
        switch (q.str.at(to)) {
          case '\a': out << 'a'; break;
          case '\b': out << 'b'; break;
          case '\t': out << 't'; break;
          case '\n': out << 'n'; break;
          case '\v': out << 'v'; break;
          case '\f': out << 'f'; break;
          case '\r': out << 'r'; break;
          case '\e': out << 'e'; break;
          case '"':  out << '"'; break;
        }
        from = to+1;
      }
      to = q.str.find_first_of(search, from);
    }
    out << q.str.substr(from);
    if (q.put_quo) out << (q.do_col ? ':' : '"');
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
