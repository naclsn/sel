#include <cmath>

#define TRACE(...)
#include "sel/builtins.hpp"
#include "sel/visitors.hpp"
#include "sel/parser.hpp"

namespace sel {

  Val* StrChunks::copy() const {
    return new StrChunks(app, chunks);
  }
  void StrChunks::accept(Visitor& v) const {
    v.visitStrChunks(type(), this->chunks);
  }

  // internal
  template <typename list> struct linear_search;
  template <typename car, typename cdr>
  struct linear_search<bins_ll::cons<car, cdr>> {
    static inline Val* the(App& app, std::string const& name) {
      if (car::name == name) return new typename car::Head(app);
      return linear_search<cdr>::the(app, name);
    }
  };
  template <>
  struct linear_search<bins_ll::nil> {
    static inline Val* the(App& app, std::string const& _) {
      return nullptr;
    }
  };

  Val* lookup_name(App& app, std::string const& name) {
    return linear_search<bins_ll::bins>::the(app, name);
  }

  // internal
  template <typename list> struct push_names_into;
  template <typename car, typename cdr>
  struct push_names_into<bins_ll::cons<car, cdr>> {
    static inline void the(std::vector<std::string>& name) {
      name.push_back(car::name);
      push_names_into<cdr>::the(name);
    }
  };
  template <>
  struct push_names_into<bins_ll::nil> {
    static inline void the(std::vector<std::string>& _) { }
  };

  unsigned list_names(std::vector<std::string>& names) {
    constexpr unsigned count = ll::count<bins_ll::bins>::the;
    names.reserve(count);
    push_names_into<bins_ll::bins>::the(names);
    return count;
  }

  namespace bins_helpers {

    template <typename Impl, typename one>
    Val* _bin_be<Impl, ll::cons<one, ll::nil>>::the::copy() const {
      typedef _bin_be<Impl, ll::cons<one, ll::nil>>::the a;
      TRACE(copyOne, a::the::Base::Next::name);
      return new typename a::Base::Next(this->app); // copyOne
    }
    template <typename Impl, typename one>
    void _bin_be<Impl, ll::cons<one, ll::nil>>::the::accept(Visitor& v) const {
      v.visit(*(Impl*)this); // visitOne
    }

    template <typename Impl, typename last_arg, char b>
    Val* _bin_be<Impl, cons<fun<last_arg, unk<b>>, nil>>::the::copy() const {
      typedef _bin_be<Impl, cons<fun<last_arg, unk<b>>, nil>>::the a;
      TRACE(copyOne2, a::the::Base::Next::name);
      return new typename a::Base::Next(this->app); // copyOne2
    }
    template <typename Impl, typename last_arg, char b>
    void _bin_be<Impl, cons<fun<last_arg, unk<b>>, nil>>::the::accept(Visitor& v) const {
      v.visit(*(Impl*)this); // visitOne2
    }

    template <typename NextT, typename to, typename from, typename from_again, typename from_more>
    Val* _bin_be<NextT, ll::cons<to, ll::cons<from, ll::cons<from_again, from_more>>>>::copy() const {
      typedef _bin_be<NextT, ll::cons<to, ll::cons<from, ll::cons<from_again, from_more>>>> a;
      TRACE(copyBody, a::the::Base::Next::name);
      return new _bin_be<NextT, ll::cons<to, ll::cons<from, ll::cons<from_again, from_more>>>>(
        this->app,
        (a::Base*)base->copy(),
        (a::Arg*)arg->copy()
      ); // copyBody
    }
    template <typename NextT, typename to, typename from, typename from_again, typename from_more>
    void _bin_be<NextT, ll::cons<to, ll::cons<from, ll::cons<from_again, from_more>>>>::accept(Visitor& v) const {
      v.visit(*this); // visitBody
    }

    template <typename NextT, typename last_to, typename last_from>
    Val* _bin_be<NextT, ll::cons<last_to, ll::cons<last_from, ll::nil>>>::_the_when_not_unk::copy() const {
      typedef _bin_be<NextT, ll::cons<last_to, ll::cons<last_from, ll::nil>>>::the a;
      TRACE(copyTail1, a::the::Base::Next::name);
      return new typename a::Base::Next(
        this->app,
        (typename a::Base*)base->copy(),
        (typename a::Arg*)arg->copy()
      ); // copyTail1
    }
    template <typename NextT, typename last_to, typename last_from>
    Val* _bin_be<NextT, ll::cons<last_to, ll::cons<last_from, ll::nil>>>::_the_when_is_unk::copy() const {
      typedef _bin_be<NextT, ll::cons<last_to, ll::cons<last_from, ll::nil>>>::the a;
      TRACE(copyTail2, a::the::Base::Next::name);
      return new typename a::Base::Next(
        this->app,
        (typename a::Base*)base->copy(),
        (typename a::Arg*)arg->copy()
      ); // copyTail2
    }
    template <typename NextT, typename last_to, typename last_from>
    void _bin_be<NextT, ll::cons<last_to, ll::cons<last_from, ll::nil>>>::the::accept(Visitor& v) const {
      v.visit(*(typename Base::Next*)this); // visitTail
    }

    template <typename NextT, typename last_to, typename last_from>
    Val* _bin_be<NextT, ll::cons<last_to, ll::cons<last_from, ll::nil>>>::copy() const {
      typedef _bin_be<NextT, ll::cons<last_to, ll::cons<last_from, ll::nil>>> a;
      TRACE(copyHead, a::the::Base::Next::name);
      return new a(this->app); // copyHead
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
// YYY: could it somehow be moved into `BIN_...`? in a way
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

    std::ostream& bin_::stream(std::ostream& out) {
      read = true;
      uint64_t a = arg->value();
      char b[64+1]; b[64] = '\0';
      char* head = b+64;
      do { *--head = '0' + (a & 1); } while (a>>= 1);
      return out << head;
    }
    bool bin_::end() const { return read; }
    std::ostream& bin_::entire(std::ostream& out) { return out << *this; }

    void conjunction_::once() {
      bind_args(l, r);
      while (!l.end()) {
        std::ostringstream oss;
        ((Str*)*l)->entire(oss);
        inleft.insert(oss.str());
        ++l;
      }
      while (!r.end()) {
        std::ostringstream oss;
        ((Str*)*r)->entire(oss);
        if (inleft.end() != inleft.find(oss.str())) break;
        ++r;
      }
      did_once = true;
    }
    Val* conjunction_::operator*() {
      bind_args(l, r);
      if (!did_once) once();
      return *r;
    }
    Lst& conjunction_::operator++() {
      bind_args(l, r);
      if (!did_once) once();
      ++r;
      while (!r.end()) {
        std::ostringstream oss;
        ((Str*)*r)->entire(oss);
        if (inleft.end() != inleft.find(oss.str())) break;
        ++r;
      }
      return *this;
    }
    bool conjunction_::end() const {
      bind_args(l, r);
      return (did_once ? inleft.empty() : l.end()) || r.end();
    }

    Val* bytes_::operator*() {
      bind_args(s);
      if (buff.length() <= off && !s.end()) {
        std::ostringstream oss;
        buff = (oss << s, oss.str());
        off = 0;
      }
      return new NumLiteral(app, buff[off]); // XXX: dont like how it makes it appear as a literal, may have a NumComputed similar to StrChunks
    }
    Lst& bytes_::operator++() {
      bind_args(s);
      off++;
      if (buff.length() <= off && !s.end()) {
        std::ostringstream oss;
        buff = (oss << s, oss.str());
        off = 0;
      }
      return *this;
    }
    bool bytes_::end() const {
      bind_args(s);
      return s.end() && buff.length() <= off;
    }

    Val* codepoints_::operator*() {
      bind_args(s);
      if (!did_once) {
        isi = std::istream_iterator<codepoint>(sis = Str_istream(&s));
        did_once = true;
      }
      return new NumLiteral(app, isi->u); // XXX: dont like how it makes it appear as a literal, may have a NumComputed similar to StrChunks
    }
    Lst& codepoints_::operator++() {
      bind_args(s);
      if (!did_once) {
        isi = std::istream_iterator<codepoint>(sis = Str_istream(&s));
        did_once = true;
      }
      isi++;
      return *this;
    }
    bool codepoints_::end() const {
      bind_args(s);
      static std::istream_iterator<codepoint> eos;
      return did_once ? eos == isi : s.end();
    }

    Val* const_::impl(LastArg& ignore) {
      bind_args(take);
      return &take;
    }

    double div_::value() {
      bind_args(a, b);
      return a.value() / b.value();
    }

    Val* drop_::operator*() {
      bind_args(n, l);
      if (!done) {
        for (size_t k = 0; k < n.value() && !l.end(); k++)
          ++l;
        done = true;
      }
      return *l;
    }
    Lst& drop_::operator++() {
      bind_args(n, l);
      if (!done) {
        for (size_t k = 0; k < n.value() && !l.end(); k++)
          ++l;
        done = true;
      }
      ++l;
      return *this;
    }
    bool drop_::end() const {
      bind_args(n, l);
      if (done) return l.end();
      size_t k;
      for (k = 0; k < n.value() && !l.end(); k++)
        ++l;
      done = true;
      return l.end() && n.value() != k;
    }

    Val* dropwhile_::operator*() {
      bind_args(p, l);
      if (!done) {
        while (!l.end() && p(*l))
          ++l;
        done = true;
      }
      return *l;
    }
    Lst& dropwhile_::operator++() {
      bind_args(p, l);
      if (!done) {
        while (!l.end() && p(*l))
          ++l;
        done = true;
      }
      ++l;
      return *this;
    }
    bool dropwhile_::end() const {
      bind_args(p, l);
      if (done) return l.end();
      while (!l.end() && p(*l))
        ++l;
      done = true;
      return l.end();
    }

    Val* duple_::operator*() {
      bind_args(v);
      return v.copy();
    }
    Lst& duple_::operator++() {
      ++did;
      return *this;
    }
    bool duple_::end() const {
      return 2 == did;
    }

    Val* filter_::operator*() {
      bind_args(p, l);
      if (!curr) {
        while (!l.end() && !((Num*)p(*l))->value()) ++l;
        curr = *l;
      }
      return *l;
    }
    Lst& filter_::operator++() {
      bind_args(p, l);
      ++l;
      while (!l.end() && !((Num*)p(*l))->value()) ++l;
      curr = *l;
      return *this;
    }
    bool filter_::end() const {
      bind_args(p, l);
      // TODO: no, there is no way to tell if we are at the
      // end without iterating until either p(*l) or l.end
      return l.end();
    }

    Val* graphemes_::operator*() {
      bind_args(s);
      if (!did_once) {
        isi = std::istream_iterator<codepoint>(sis = Str_istream(&s));
        did_once = true;
        read_grapheme(isi, curr);
      }
      std::ostringstream oss;
      return new StrChunks(app, (oss << curr, oss.str()));
    }
    Lst& graphemes_::operator++() {
      bind_args(s);
      if (!did_once) {
        isi = std::istream_iterator<codepoint>(sis = Str_istream(&s));
        did_once = true;
      }
      curr.clear();
      static std::istream_iterator<codepoint> const eos;
      if (eos == isi) past_end = true;
      else read_grapheme(isi, curr);
      return *this;
    }
    bool graphemes_::end() const {
      bind_args(s);
      static std::istream_iterator<codepoint> const eos;
      return did_once ? eos == isi && past_end : s.end();
    }

    std::ostream& hex_::stream(std::ostream& out) { read = true; return out << std::hex << size_t(arg->value()); }
    bool hex_::end() const { return read; }
    std::ostream& hex_::entire(std::ostream& out) { read = true; return out << std::hex << size_t(arg->value()); }

    Val* flip_::impl(LastArg& a) {
      bind_args(fun, b);
      return (*(Fun*)fun(&a))(&b);
    }

    Val* id_::impl(LastArg& take) {
      return &take;
    }

    Val* if_::impl(LastArg& argument) {
      bind_args(condition, consequence, alternative);
      return ((Num*)condition(&argument))->value()
        ? &consequence
        : &alternative;
    }

    Val* index_::impl(LastArg& k) {
      if (!did) {
        bind_args(l);
        const size_t idx = k.value();
        size_t len;
        for (len = 0; !l.end() && len < idx; ++l, len++);
        if (idx == len) {
          found = *l;
        } else {
          std::ostringstream oss;
          throw RuntimeError((oss << "index out of range: " << idx << " but length is " << len, oss.str()));
        }
        did = true;
      }
      return found;
    }

    Val* iterate_::operator*() {
      bind_args(f, o);
      return !curr ? &o : curr;
    }
    Lst& iterate_::operator++() {
      bind_args(f, o);
      curr = f(!curr ? &o : curr);
      return *this;
    }
    bool iterate_::end() const { return false; }

    std::ostream& join_::stream(std::ostream& out) {
      bind_args(sep, lst);
      if (beginning) {
        std::ostringstream oss;
        sep.entire(oss);
        ssep = oss.str();
        beginning = false;
      } else out << ssep;
      Str* it = (Str*)*lst;
      it->entire(out);
      ++lst;
      return out;
    }
    bool join_::end() const {
      bind_args(sep, lst);
      return lst.end();
    }
    std::ostream& join_::entire(std::ostream& out) {
      bind_args(sep, lst);
      if (lst.end()) return out;
      if (beginning) {
        std::ostringstream oss;
        sep.entire(oss);
        ssep = oss.str();
        beginning = false;
      }
      // first iteration unrolled (because no separator)
      ((Str*)*lst)->entire(out);
      ++lst;
      for (; !lst.end(); ++lst) {
        ((Str*)*lst)->entire(out << ssep);
      }
      return out;
    }

    Val* map_::operator*() {
      bind_args(f, l);
      return f(*l);
    }
    Lst& map_::operator++() {
      bind_args(f, l);
      ++l;
      return *this;
    }
    bool map_::end() const {
      bind_args(f, l);
      return l.end();
    }

    double mul_::value() {
      bind_args(a, b);
      return a.value() * b.value();
    }

    std::ostream& ln_::stream(std::ostream& out) {
      bind_args(s);
      return !s.end() ? out << s : (done = true, out << '\n');
    }
    bool ln_::end() const { return done; }
    std::ostream& ln_::entire(std::ostream& out) {
      bind_args(s);
      done = true;
      return s.entire(out) << '\n';
    }

    double pi_::value() {
      return M_PI;
    }

    std::ostream& oct_::stream(std::ostream& out) { read = true; return out << std::oct << size_t(arg->value()); }
    bool oct_::end() const { return read; }
    std::ostream& oct_::entire(std::ostream& out) { read = true; return out << std::oct << size_t(arg->value()); }

    Val* repeat_::operator*() { return arg->copy(); }
    Lst& repeat_::operator++() { return *this; }
    bool repeat_::end() const { return false; }

    Val* replicate_::operator*() {
      if (!did) did++;
      bind_args(n, o);
      return o.copy();
    }
    Lst& replicate_::operator++() {
      did++;
      return *this;
    }
    bool replicate_::end() const {
      bind_args(n, o);
      return 0 == n.value() || n.value() < did;
    }

    void reverse_::once() {
      bind_args(l);
      for (; !l.end(); ++l)
        cache.push_back(*l);
      did_once = true;
      curr = cache.size();
    }
    Val* reverse_::operator*() {
      if (!did_once) once();
      return cache[curr-1];
    }
    Lst& reverse_::operator++() {
      curr--;
      return *this;
    }
    bool reverse_::end() const {
      if (did_once) return 0 == curr;
      bind_args(l);
      return l.end();
    }

    Val* singleton_::operator*() { return arg; }
    Lst& singleton_::operator++() { done = true; return *this; }
    bool singleton_::end() const { return done; }

    void split_::once() {
      bind_args(sep, str);
      std::ostringstream oss;
      sep.entire(oss);
      ssep = oss.str();
      did_once = true;
    }
    void split_::next() {
      if (at_end) {
        at_past_end = true;
        return;
      }
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
      return new StrChunks(app, curr);
    }
    Lst& split_::operator++() {
      if (!init) { next(); init = true; }
      next();
      return *this;
    }
    bool split_::end() const {
      return at_past_end;
    }

    double startswith_::value() {
      if (!done) {
        bind_args(prefix, str);
        std::ostringstream osspx;
        osspx << prefix;
        std::string px = osspx.str();
        std::ostringstream oss;
        // BOF: copies! copies everywhere~! (or does it? sais its a temporary object...)
        while (oss.str().length() < px.length() && 0 == px.compare(0, oss.str().length(), oss.str()) && !str.end()) {
          oss << str;
        }
        does = 0 == oss.str().compare(0, px.length(), px);
        done = true;
      }
      return does;
    }

    double sub_::value() {
      bind_args(a, b);
      return a.value() - b.value();
    }

    Val* take_::operator*() {
      if (!did) did++;
      bind_args(n, l);
      return *l;
    }
    Lst& take_::operator++() {
      bind_args(n, l);
      did++;
      ++l;
      return *this;
    }
    bool take_::end() const {
      bind_args(n, l);
      auto x = n.value();
      return 0 == x || x < did || l.end();
    }

    Val* takewhile_::operator*() {
      bind_args(p, l);
      return *l;
    }
    Lst& takewhile_::operator++() {
      bind_args(p, l);
      ++l;
      return *this;
    }
    bool takewhile_::end() const {
      bind_args(p, l);
      return l.end() || !((Num*)p(*l))->value();
    }

    double tonum_::value() {
      if (!done) {
        bind_args(s);
        Str_istream(&s) >> r;
        done = true;
      }
      return r;
    }

    std::ostream& tostr_::stream(std::ostream& out) { read = true; return out << arg->value(); }
    bool tostr_::end() const { return read; }
    std::ostream& tostr_::entire(std::ostream& out) { read = true; return out << arg->value(); }

    Val* tuple_::operator*() {
      bind_args(a, b);
      return 0 == did ? &a : &b;
    }
    Lst& tuple_::operator++() {
      ++did;
      return *this;
    }
    bool tuple_::end() const {
      return 2 == did;
    }

    std::ostream& unbytes_::stream(std::ostream& out) {
      bind_args(l);
      char b = ((Num*)*l)->value();
      ++l;
      return out << b;
    }
    bool unbytes_::end() const {
      bind_args(l);
      return l.end();
    }
    std::ostream& unbytes_::entire(std::ostream& out) {
      bind_args(l);
      for (; !l.end(); ++l)
        out << char(((Num*)*l)->value());
      return out;
    }

    std::ostream& uncodepoints_::stream(std::ostream& out) {
      bind_args(l);
      codepoint cp = ((Num*)*l)->value();
      ++l;
      return out << cp;
    }
    bool uncodepoints_::end() const {
      bind_args(l);
      return l.end();
    }
    std::ostream& uncodepoints_::entire(std::ostream& out) {
      bind_args(l);
      for (; !l.end(); ++l)
        out << codepoint(((Num*)*l)->value());
      return out;
    }

    Val* uncurry_::impl(LastArg& pair) {
      bind_args(f);
      return (*(Fun*)f(*pair))(*++pair);
    }

    Val* zipwith_::operator*() {
      bind_args(f, l1, l2);
      if (!curr) curr = (*(Fun*)f(*l1))(*l2);
      return curr;
    }
    Lst& zipwith_::operator++() {
      bind_args(f, l1, l2);
      curr = nullptr;
      ++l1;
      ++l2;
      return *this;
    }
    bool zipwith_::end() const {
      bind_args(f, l1, l2);
      return l1.end() || l2.end();
    }

  } // namespace bins

} // namespace sel
