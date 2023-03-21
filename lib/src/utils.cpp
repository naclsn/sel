#include "sel/engine.hpp"
#include "sel/utils.hpp"
#include "sel/visitors.hpp"

using namespace std;

namespace sel { namespace utils {

  ostream& operator<<(ostream& out, raw ptr) {
    ssize_t at = (ssize_t)ptr.ptr;

    int on_stack;
    int* stack = &on_stack;
    int* heap = new int;

    ssize_t to_stack = abs((ssize_t)stack - at);
    ssize_t to_heap = abs((ssize_t)heap - at);

    delete heap;
    char buf[16];
    sprintf(buf, "0x%lX", at);

    if (to_stack < to_heap)
      return out << "\e[35m" << buf << "\e[m";
    else
      return out << "\e[33m" << buf << "\e[m";
  }

  ostream& operator<<(ostream& out, repr me) {
    VisRepr repr(out, {
      .single_line= me.single_line,
      .no_recurse= me.no_recurse,
      .only_class= me.only_class,
    });
    me.val.accept(repr);
    return out;
  }

  ostream& operator<<(ostream& out, quoted q) {
    if (q.put_quo) out << (q.do_col ? ':' : '"');
    string const search =
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
    string::size_type from = 0, to = q.str.find_first_of(search);
    while (string::npos != to) {
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

} } // sel::utils
