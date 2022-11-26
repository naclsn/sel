#include "sel/builtins.hpp"
#include "sel/visitors.hpp"

namespace sel {

  // internal
  template <typename list> struct linear_search;
  template <typename car, typename cdr>
  struct linear_search<bins_ll::cons<car, cdr>> {
    static inline Val* the(std::string const& name) {
      if (car::name == name) return new typename car::Head();
      return linear_search<cdr>::the(name);
    }
  };
  template <>
  struct linear_search<bins_ll::nil> {
    static inline Val* the(std::string const& _) {
      return nullptr;
    }
  };

  Val* lookup_name(std::string const& name) {
    return linear_search<bins_ll::bins>::the(name);
  }

  namespace bins_helpers {

    template <typename NextT, typename to, typename from, typename from_again, typename from_more>
    void _bin_be<NextT, ll::cons<to, ll::cons<from, ll::cons<from_again, from_more>>>>::accept(Visitor& v) const {
      v.visit(*this); // visitBody
    }

    template <typename NextT, typename last_to, typename last_from>
    void _bin_be<NextT, ll::cons<last_to, ll::cons<last_from, ll::nil>>>::the::accept(Visitor& v) const {
      v.visit(*(typename Base::Next*)this); // visitTail
    }

    template <typename NextT, typename last_to, typename last_from>
    void _bin_be<NextT, ll::cons<last_to, ll::cons<last_from, ll::nil>>>::accept(Visitor& v) const {
      v.visit(*this); // visitHead
    }

  } // namespace bins_helpers

#define _depth(__depth) _depth_ ## __depth
#define _depth_0 arg
#define _depth_1 base->_depth_0
#define _depth_2 base->_depth_1
#define _depth_3 base->_depth_2

#define _bind_some(__count) _bind_some_ ## __count
#define _bind_some_1(a)          _bind_one(a, 0)
#define _bind_some_2(a, b)       _bind_one(a, 1); _bind_some_1(b)
#define _bind_some_3(a, b, c)    _bind_one(a, 2); _bind_some_2(b, c)
#define _bind_some_4(a, b, c, d) _bind_one(a, 3); _bind_some_3(b, c, d)

#define _bind_count(__count, ...) _bind_some(__count)(__VA_ARGS__)
#define _bind_one(__name, __depth) auto& __name = *this->_depth(__depth); (void)__name
// YYY: could it somehow be moved into `BODY`? in a way
// that it is only written once and the named arg refs
// are available all throughout the struct
#define bind_args(...) _bind_count(__VA_COUNT(__VA_ARGS__), __VA_ARGS__)

  namespace bins {

    double abs_::value() {
      return std::abs(arg->value());
    }

    double add_::value() {
      bind_args(a, b);
      return a.value() + b.value();
    }

    Val* map_::operator*() {
      bind_args(f, l);
      if (!curr) curr = f(*l);
      return curr;
    }
    Lst& map_::operator++() {
      bind_args(f, l);
      curr = nullptr;
      return ++l;
    }
    bool map_::end() const {
      bind_args(f, l);
      return l.end();
    }
    void map_::rewind() {
      bind_args(f, l);
      l.rewind();
    }
    size_t map_::count() {
      bind_args(f, l);
      return l.count();
    }

    Val* flip_::impl() {
      bind_args(fun, b, a);
      return (*(Fun*)fun(&a))(&b);
    }

    std::ostream& join_::stream(std::ostream& out) {
      bind_args(sep, lst);
      if (beginning) beginning = false;
      else sep.entire(out);
      Str* it = (Str*)*lst;
      it->entire(out);
      ++lst;
      return out;
    }
    bool join_::end() const {
      bind_args(sep, lst);
      return lst.end();
    }
    void join_::rewind() {
      bind_args(sep, lst);
      lst.rewind();
      beginning = true;
    }
    std::ostream& join_::entire(std::ostream& out) {
      bind_args(sep, lst);
      if (lst.end()) return out;
      out << *(Str*)(*lst);
      while (!lst.end()) {
        sep.rewind();
        sep.entire(out);
        ++lst;
        out << *(Str*)(*lst);
      }
      return out;
    }

    // TODO: lol
    Val* repeat_::operator*() { return nullptr; }
    Lst& repeat_::operator++() { return *this; }
    bool repeat_::end() const { return true; }
    void repeat_::rewind() { }
    size_t repeat_::count() { return 0; }

    void split_::once() {
      bind_args(sep, str);
      std::ostringstream oss;
      sep.entire(oss);
      ssep = oss.str();
      did_once = true;
    }
    void split_::next() {
      if (!did_once) once();
      bind_args(sep, str);
      std::string buf = acc.str();
      std::string::size_type at = buf.find(ssep);
      if (std::string::npos != at) {
        // found in current acc, pop(0)
        curr = buf.substr(0, at);
        acc = std::ostringstream(buf.substr(at+ssep.size()));
        return;
      }
      if (str.end()) {
        // send the rest of acc, set end
        curr = buf;
        at_end = true;
        return;
      }
      acc << str;
      return next();
    }
    Val* split_::operator*() {
      if (!init) { next(); init = true; }
      return new StrChunks(curr);
    }
    Lst& split_::operator++() {
      if (!init) { next(); init = true; }
      next();
      return *this;
    }
    bool split_::end() const {
      return at_end;
    }
    void split_::rewind() {
      bind_args(sep, str);
      str.rewind();
      acc = std::ostringstream(std::ios_base::ate);
      at_end = false;
      init = false;
    }
    size_t split_::count() {
      throw NIYError("size_t count()", "- what -");
    }

    double sub_::value() {
      bind_args(a, b);
      return a.value() - b.value();
    }

    double tonum_::value() {
      bind_args(s);
      double r;
      std::stringstream ss;
      s.entire(ss);
      ss >> r;
      return r;
    }

    std::ostream& tostr_::stream(std::ostream& out) { read = true; return out << arg->value(); }
    bool tostr_::end() const { return read; }
    void tostr_::rewind() { read = false; }
    std::ostream& tostr_::entire(std::ostream& out) { read = true; return out << arg->value(); }

    Val* zipwith_::operator*() { return nullptr; }
    Lst& zipwith_::operator++() { return *this; }
    bool zipwith_::end() const { return true; }
    void zipwith_::rewind() { }
    size_t zipwith_::count() { return 0; }

  } // namespace bins

} // namespace sel
