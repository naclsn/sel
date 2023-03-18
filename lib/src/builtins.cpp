#include <cmath>

#include "sel/builtins.hpp"
#include "sel/parser.hpp"
#include "sel/utils.hpp"
#include "sel/visitors.hpp"

namespace sel {

  handle<Val> lookup_name(App& app, std::string const& name) {
    auto itr = bins_list::map.find(name);
    if (bins_list::map.end() == itr) return null_handle;
    auto f = (*itr).second;
    return f(app);
  }

  // helpful templates to keep C++-side typing
  template <typename U>
  static inline handle<U> clone(U& a) { return a.copy(); }
  template <typename U>
  static inline handle<U> clone(handle<U> a) { return a->copy(); }

  // YYY: not used yet
  template <typename P, typename R>
  static inline handle<R> call(bins_helpers::fun<P, R>& f, handle<P> p) { return f(p); }
  template <typename P, typename R>
  static inline handle<R> call(handle<bins_helpers::fun<P, R>> f, handle<P> p) { return (*f)(p); }

  // YYY: not used yet
  template <bool is_inf, bool is_tpl, typename I>
  static inline handle<I> next(bins_helpers::_lst<is_inf, is_tpl, I>& l) { return ++l; }
  template <bool is_inf, bool is_tpl, typename I>
  static inline handle<I> next(handle<bins_helpers::_lst<is_inf, is_tpl, I>> l) { return ++*l; }

#define _bind_some(__count) _bind_some_ ## __count
#define _bind_some_1(a)          _bind_one(a, 0)
#define _bind_some_2(a, b)       _bind_one(a, 1); _bind_some_1(b)
#define _bind_some_3(a, b, c)    _bind_one(a, 2); _bind_some_2(b, c)
#define _bind_some_4(a, b, c, d) _bind_one(a, 3); _bind_some_3(b, c, d)

#define _bind_count(__count, ...) _bind_some(__count)(__VA_ARGS__)
#define _bind_one(__name, __depth) auto& __name = *std::get<argsN-1 - __depth>(_args); (void)__name
// YYY: could it somehow be moved into `BIN_...`? in a way
// that it is only written once and the named arg refs
// are available all throughout the struct
#define bind_args(...) _bind_count(__VA_COUNT(__VA_ARGS__), __VA_ARGS__)

  namespace bins {

    double abs_::value() {
      bind_args(n);
      double r = std::abs(n.value());
      // n.drop(); -> drop(0); // TODO: everywhere
      hold<NumResult>(r);
      return r;
    }

    double add_::value() {
      bind_args(a, b);
      double r = a.value() + b.value();
      // a.drop();
      // b.drop();
      hold<NumResult>(r);
      return r;
    }

    std::ostream& bin_::stream(std::ostream& out) {
      read = true;
      uint64_t a = std::get<0>(_args)->value();
      char b[64+1]; b[64] = '\0';
      char* head = b+64;
      do { *--head = '0' + (a & 1); } while (a>>= 1);
      return out << head;
    }
    bool bin_::end() { return read; }
    std::ostream& bin_::entire(std::ostream& out) { return out << *this; }

    // void conjunction_::once() {
    //   bind_args(l, r);
    //   while (!l.end()) {
    //     std::ostringstream oss;
    //     ((handle<Str>)*l)->entire(oss);
    //     inleft.insert(oss.str());
    //     ++l;
    //   }
    //   while (!r.end()) {
    //     std::ostringstream oss;
    //     ((handle<Str>)*r)->entire(oss);
    //     if (inleft.end() != inleft.find(oss.str())) break;
    //     ++r;
    //   }
    //   did_once = true;
    // }
    // handle<Val> conjunction_::operator*() {
    //   bind_args(l, r);
    //   if (!did_once) once();
    //   return *r;
    // }
    // Lst& conjunction_::operator++() {
    //   bind_args(l, r);
    //   if (!did_once) once();
    //   ++r;
    //   while (!r.end()) {
    //     std::ostringstream oss;
    //     ((handle<Str>)*r)->entire(oss);
    //     if (inleft.end() != inleft.find(oss.str())) break;
    //     ++r;
    //   }
    //   return *this;
    // }
    // bool conjunction_::end() {
    //   bind_args(l, r);
    //   return (did_once ? inleft.empty() : l.end()) || r.end();
    // }

    handle<Val> bytes_::operator++() {
      bind_args(s);
      if (buff.length() == off) {
        if (s.end()) return null_handle;
        std::ostringstream oss;
        buff = (oss << s, oss.str());
        off = 0;
      }
      return handle<NumResult>(h.app(), (uint8_t)buff[off++]);
    }

    std::ostream& chr_::stream(std::ostream& out) { read = true; return out << codepoint(std::get<0>(_args)->value()); }
    bool chr_::end() { return read; }
    std::ostream& chr_::entire(std::ostream& out) { read = true; return out << codepoint(std::get<0>(_args)->value()); }

    handle<Val> codepoints_::operator++() {
      static std::istream_iterator<codepoint> eos;
      if (eos == isi) return null_handle;
      double r = isi->u;
      ++isi;
      return handle<NumResult>(h.app(), r);
    }

    handle<Val> const_::operator()(handle<Val> ignore) {
      bind_args(take);
      return &take;
    }

    double contains_::value() {
      bind_args(substr, str);
      std::ostringstream ossss;
      substr.entire(ossss);
      // substr.drop();
      std::string ss = ossss.str();
      auto sslen = ss.length();
      bool does = false;
      std::ostringstream oss;
      while (!does && !str.end()) { // YYY: same as endswith_, this could do with a `sslen`-long circular buffer
        auto plen = oss.str().length();
        oss << str;
        auto len = oss.str().length();
        if (sslen <= len) { // have enough that searching is relevant
          std::string s = oss.str();
          for (auto k = plen < sslen ? sslen : plen; k <= len && !does; k++) { // only test over the range that was added
            does = 0 == s.compare(k-sslen, sslen, ss);
          }
        }
      }
      // str.drop();
      hold<NumResult>(does);
      return does;
    }

    double count_::value() {
      bind_args(matchit, l);
      std::ostringstream search; ((Str&)matchit).entire(search); // XXX
      // matchit.drop();
      size_t n = 0;
      for (auto it = ++l; it; it = ++l) {
        std::ostringstream item; ((handle<Str>)it)->entire(item); // XXX
        it.drop();
        if (search.str() == item.str())
          n++;
      }
      // l.drop();
      hold<NumResult>(n);
      return n;
    }

    double div_::value() {
      bind_args(a, b);
      double r = a.value() / b.value();
      // a.drop();
      // b.drop();
      hold<NumResult>(r);
      return r;
    }

    handle<Val> drop_::operator++() {
      bind_args(n, l);
      if (!done) {
        done = true;
        size_t k = 0;
        size_t const m = n.value();
        // n.drop();
        for (auto it = ++l; it; it = ++l) {
          if (m == k++) return it;
          it->drop();
        }
      }
      return ++l;
    }

    handle<Val> dropwhile_::operator++() {
      bind_args(p, l);
      if (!done) {
        done = true;
        for (auto it = next(l); it; it = ++l) {
          // YYY: auto n = call(clone(p), clone(it))
          handle<Num> n = (*clone(p))(it->copy());
          double still_dropping = 0 == n->value();
          n->drop();
          if (!still_dropping) {
            // drop p
            return it;
          }
        }
      }
      return ++l;
    }

    handle<Val> duple_::operator++() {
      if (0 == did++) return std::get<0>(_args)->copy();
      if (1 == did) {
        handle<Val> r = std::get<0>(_args);
        std::get<0>(_args) = null_handle;
        return r;
      }
      return null_handle;
    }

    double endswith_::value() {
      bind_args(suffix, str);
      std::ostringstream osssx;
      suffix.entire(osssx);
      // drop suffix
      std::string sx = osssx.str();
      auto sxlen = sx.length();
      std::ostringstream oss;
      while (!str.end()) { // YYY: that's essentially '::entire', but endswith_ could leverage a circular buffer...
        oss << str;
      }
      // drop str
      std::string s = oss.str();
      bool does = sxlen <= s.length() && 0 == s.compare(s.length()-sxlen, sxlen, sx);
      hold<NumResult>(does);
      return does;
    }

    handle<Val> filter_::operator++() {
      bind_args(p, l);
      for (auto it = ++l; it; it = ++l) {
        handle<Num> n = (*clone(p))(it->copy());
        bool is = 0 != n->value();
        n->drop();
        if (is) return it;
      }
      // drop p and l
      return null_handle;
    }

    handle<Val> flip_::operator()(handle<Val> a) {
      bind_args(fun, b);
      // next consumes everything; needs to place null_handles for fun and b
      return (*(handle<Fun>)fun(a))(&b);
    }

    // XXX: at delete, drop rest of buffer
    handle<Val> give_::operator++() {
      bind_args(n, l);
      if (0 == circ.capacity()) {
        size_t count = n.value();
        // drop n
        if (0 == count) {
          // drop l
          return null_handle;
        }
        circ.reserve(count);
        for (auto it = ++l; it; it = ++l) {
          circ.push_back(it);
          // buffer filled, send the first one
          if (circ.size() == count) return circ[at++];
        }
        // here if the list was smaller than the count
        // drop l
        return null_handle;
      }
      if (circ.size() == at) at = 0;
      handle<Val> r = circ[at];
      circ[at++] = ++l;
      return r;
    }

    handle<Val> graphemes_::operator++() {
      static std::istream_iterator<codepoint> const eos;
      if (eos == isi) return null_handle;
      grapheme curr;
      read_grapheme(isi, curr);
      std::ostringstream oss;
      return handle<StrChunks>(h.app(), (oss << curr, oss.str()));
    }

    handle<Val> head_::operator()(handle<Val> l) {
      auto it = ++*(handle<Lst>)l;
      if (!it) throw RuntimeError("head of empty list");
      // drop l
      // swap with it
      return it;
    }

    std::ostream& hex_::stream(std::ostream& out) { read = true; return out << std::hex << size_t(std::get<0>(_args)->value()); }
    bool hex_::end() { return read; }
    std::ostream& hex_::entire(std::ostream& out) { read = true; return out << std::hex << size_t(std::get<0>(_args)->value()); }

    handle<Val> id_::operator()(handle<Val> take) {
      // swap with take
      return take;
    }

    handle<Val> if_::operator()(handle<Val> argument) {
      bind_args(condition, consequence, alternative);
      handle<Num> n = condition(argument);
      bool is = n->value();
      n.drop();
      // swap with one, drop the other
      return is ? &consequence : &alternative;
    }

    handle<Val> index_::operator()(handle<Val> _k) {
      bind_args(l); auto& k = *(handle<Num>)_k;
      size_t len = 0;
      size_t const idx = k.value();
      k.drop();
      for (auto it = ++l; it; it = ++l) {
        if (idx == len++) {
          auto r = h;
          // l.drop(); swap(it); drop();
          return r;
        }
      }
      // drop l
      std::ostringstream oss;
      throw RuntimeError((oss << "index out of range: " << idx << " but length is " << len, oss.str()));
    }

    // XXX
    handle<Val> init_::operator++() {
      if (finished) return null_handle;
      bind_args(l);
      handle<Val> r = prev;
      prev = ++l;
      if (!r) {
        if (!prev) throw RuntimeError("init of empty list");
        r = prev;
        prev = ++l;
      }
      if (!prev) {
        r.drop();
        finished = true;
        return null_handle;
      }
      return r;
    }

    handle<Val> iterate_::operator++() {
      bind_args(f, o);
      if (first) {
        first = false;
        return clone(o);
      }
      return (std::get<1>(_args) = (*clone(f))(&o))->copy();
    }

    std::ostream& join_::stream(std::ostream& out) {
      bind_args(sep, lst);
      if (beginning) {
        std::ostringstream oss;
        sep.entire(oss);
        // drop sep
        ssep = oss.str();
        beginning = false;
      } else out << ssep;
      handle<Str> it = ++lst;
      if (!it) {
        finished = true;
        // drop lst
      } else {
        it->entire(out);
        it.drop();
      }
      return out;
    }
    bool join_::end() {
      return finished;
    }
    std::ostream& join_::entire(std::ostream& out) {
      bind_args(sep, lst);
      handle<Str> it = ++lst;
      if (!it) return out;
      it->entire(out);
      it.drop();
      std::ostringstream oss;
      sep.entire(oss);
      // drop sep
      ssep = oss.str();
      while ((it = ++lst)) {
        out << ssep;
        it->entire(out);
        it.drop();
      }
      return out;
    }

    handle<Val> last_::operator()(handle<Val> _l) {
      auto& l = *(handle<Lst>)_l;
      auto it = ++l;
      if (!it) throw RuntimeError("last of empty list");
      auto prev = it;
      while ((it = ++l)) {
        prev.drop();
        prev = it;
      }
      return prev;
    }

    handle<Val> map_::operator++() {
      bind_args(f, l);
      auto it = ++l;
      if (!it) return it;
      return (*clone(f))(it);
    }

    double mul_::value() {
      bind_args(a, b);
      double r = a.value() * b.value();
      // drop a and b
      hold<NumResult>(r);
      return r;
    }

    std::ostream& ln_::stream(std::ostream& out) {
      bind_args(s);
      return !s.end() ? out << s : (done = true, out << '\n');
    }
    bool ln_::end() { return done; }
    std::ostream& ln_::entire(std::ostream& out) {
      bind_args(s);
      done = true;
      return s.entire(out) << '\n';
    }

    double pi_::value() {
      return M_PI;
    }

    std::ostream& oct_::stream(std::ostream& out) { read = true; return out << std::oct << size_t(std::get<0>(_args)->value()); }
    bool oct_::end() { return read; }
    std::ostream& oct_::entire(std::ostream& out) { read = true; return out << std::oct << size_t(std::get<0>(_args)->value()); }

    double ord_::value() {
      if (!done) {
        codepoint c;
        Str_streambuf sb(std::get<0>(_args));
        std::istream(&sb) >> c;
        r = c.u;
        done = true;
      }
      return r;
    }

    // XXX: drops
    std::ostream& prefix_::stream(std::ostream& out) {
      bind_args(px, s);
      return !px.end()
        ? out << px
        : out << s;
    }
    bool prefix_::end() {
      bind_args(px, s);
      return px.end() && s.end();
    }
    std::ostream& prefix_::entire(std::ostream& out) {
      bind_args(px, s);
      return s.entire(px.entire(out));
    }

    handle<Val> repeat_::operator++() { return std::get<0>(_args)->copy(); }

    handle<Val> replicate_::operator++() {
      bind_args(n, o);
      if (n.value() == did++) return null_handle;
      return std::get<0>(_args)->copy();
    }

    // XXX: at delete, drop rest of buffer
    handle<Val> reverse_::operator++() {
      bind_args(l);
      if (!did_once) {
        did_once = true;
        for (auto it = ++l; it; it = ++l)
          cache.push_back(it);
        curr = cache.size();
      }
      if (0 == curr) return null_handle;
      return cache[--curr];
    }

    handle<Val> singleton_::operator++() {
        if (done) return null_handle;
        return std::get<0>(_args);
    }

    // XXX
    void split_::once() {
      bind_args(sep, str);
      std::ostringstream oss;
      sep.entire(oss);
      // drop sep
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
        // drop str
        return;
      }
      acc << str;
      return next();
    }
    handle<Val> split_::operator++() {
      next();
      if (at_past_end) return null_handle;
      return handle<StrChunks>(h.app(), curr);
    }

    double startswith_::value() {
      bind_args(prefix, str);
      std::ostringstream osspx;
      prefix.entire(osspx);
      // drop prefix
      std::string px = osspx.str();
      std::ostringstream oss;
      while (oss.str().length() < px.length() && 0 == px.compare(0, oss.str().length(), oss.str()) && !str.end()) {
        oss << str;
      }
      // drop str
      bool does = 0 == oss.str().compare(0, px.length(), px);
      hold<NumResult>(does);
      return does;
    }

    double sub_::value() {
      bind_args(a, b);
      double r = a.value() - b.value();
      // drop a and b
      hold<NumResult>(r);
      return r;
    }

    // XXX: drops
    std::ostream& suffix_::stream(std::ostream& out) {
      bind_args(sx, s);
      return !s.end()
        ? out << s
        : out << sx;
    }
    bool suffix_::end() {
      bind_args(sx, s);
      return s.end() && sx.end();
    }
    std::ostream& suffix_::entire(std::ostream& out) {
      bind_args(sx, s);
      return sx.entire(s.entire(out));
    }

    // XXX: drops
    std::ostream& surround_::stream(std::ostream& out) {
      bind_args(px, sx, s);
      return !px.end()
        ? out << px : !s.end()
        ? out << s
        : out << sx;
    }
    bool surround_::end() {
      bind_args(px, sx, s);
      return px.end() && s.end() && sx.end();
    }
    std::ostream& surround_::entire(std::ostream& out) {
      bind_args(px, sx, s);
      return sx.entire(s.entire(px.entire(out)));
    }

    handle<Val> tail_::operator++() {
      bind_args(l);
      if (!done) {
        done = true;
        auto it = ++l;
        if (!it) throw RuntimeError("tail of empty list");
        it.drop();
      }
      return ++l;
    }

    handle<Val> take_::operator++() {
      bind_args(n, l);
      if (n.value() == did) {
        // drop n (but in that case need to cache) and l
        return null_handle;
      }
      did++;
      return ++l;
    }

    handle<Val> takewhile_::operator++() {
      if (finished) return null_handle;
      bind_args(p, l);
      auto it = ++l;
      if (!it) return it; // now can drop
      handle<Num> n = (*clone(p))(it->copy());
      bool still_taking = n->value();
      n.drop();
      if (still_taking) return it;
      it.drop();
      finished = true;
      // now can drop
      return null_handle;
    }

    double tonum_::value() {
      Str_streambuf sb(std::get<0>(_args));
      double r;
      std::istream(&sb) >> r;
      // drop arg 0
      hold<NumResult>(r);
      return r;
    }

    std::ostream& tostr_::stream(std::ostream& out) { read = true; return out << std::get<0>(_args)->value(); }
    bool tostr_::end() { return read; }
    std::ostream& tostr_::entire(std::ostream& out) { read = true; return out << std::get<0>(_args)->value(); }

    handle<Val> tuple_::operator++() {
      bind_args(a, b);
      if (0 == did++) return &a;
      if (1 == did++) return &b;
      return null_handle;
    }

    double unbin_::value() {
      bind_args(s);
      size_t n = 0;
      while (!s.end()) {
        std::ostringstream oss;
        std::string buff = (oss << s, oss.str());
        for (char const c : buff) {
          if ('0' != c && '1' != c) goto out;
          n = (n << 1) | (c - '0');
        }
      }
    out:
      // s.drop();
      hold<NumResult>(n);
      return n;
    }

    std::ostream& unbytes_::stream(std::ostream& out) {
      bind_args(l);
      if (handle<Num> it = ++l) {
        out << (char)it->value();
        it.drop();
      } else finished = true;
      return out;
    }
    bool unbytes_::end() { return finished; }
    std::ostream& unbytes_::entire(std::ostream& out) {
      bind_args(l);
      for (handle<Num> it = ++l; it; it = ++l) {
        out << (char)it->value();
        it.drop();
      }
      return out;
    }

    std::ostream& uncodepoints_::stream(std::ostream& out) {
      bind_args(l);
      if (handle<Num> it = ++l) {
        out << codepoint(it->value());
        it.drop();
      } else finished = true;
      return out;
    }
    bool uncodepoints_::end() { return finished; }
    std::ostream& uncodepoints_::entire(std::ostream& out) {
      bind_args(l);
      for (handle<Num> it = ++l; it; it = ++l) {
        out << codepoint(it->value());
        it.drop();
      }
      return out;
    }

    handle<Val> uncurry_::operator()(handle<Val> _pair) {
      auto& pair = *(handle<Lst>)_pair;
      bind_args(f);
      auto a = ++pair;
      auto b = ++pair;
      // swap with and drop self
      return (*(handle<Fun>)f(a))(b);
    }

    double unhex_::value() {
      size_t n = 0;
      Str_streambuf sb(std::get<0>(_args));
      std::istream(&sb) >> std::hex >> n;
      // drop arg 0
      hold<NumResult>(n);
      return n;
    }

    double unoct_::value() {
      size_t n = 0;
      Str_streambuf sb(std::get<0>(_args));
      std::istream(&sb) >> std::oct >> n;
      // drop arg 0
      hold<NumResult>(n);
      return n;
    }

    handle<Val> zipwith_::operator++() {
      bind_args(f, l1, l2);
      auto a = ++l1;
      auto b = ++l2;
      if (!a || !b) return a;
      return (*(handle<Fun>)(*clone(f))(a))(b);
    }

  } // namespace bins

// YYY: somehow, this seems to only be needed with BINS_MIN
#ifdef BINS_MIN
  namespace bins {
    constexpr char const* const add_::name;
    constexpr char const* const codepoints_::name;
    constexpr char const* const div_::name;
    constexpr char const* const flip_::name;
    constexpr char const* const graphemes_::name;
    constexpr char const* const index_::name;
    constexpr char const* const join_::name;
    constexpr char const* const mul_::name;
    constexpr char const* const sub_::name;
    constexpr char const* const tonum_::name;
    constexpr char const* const tostr_::name;
  }
#endif

  template <typename PackItself> struct _make_bins_list;
  template <typename ...Pack>
  struct _make_bins_list<ll::pack<Pack...>> {
    static std::unordered_map<std::string, handle<Val> (*)(App&)> const map;
    static char const* const names[];
    constexpr static size_t count = sizeof...(Pack);
  };

  template <typename ...Pack>
  constexpr char const* const _make_bins_list<ll::pack<Pack...>>::names[] = {Pack::name...};

  template <typename Va>
  handle<Val> _bin_new(App& app) { return handle<typename Va::Head>(app); }
  // XXX: static constructor
  template <typename ...Pack>
  std::unordered_map<std::string, handle<Val> (*)(App&)> const _make_bins_list<ll::pack<Pack...>>::map = {{Pack::name, _bin_new<Pack>}...};

  std::unordered_map<std::string, handle<Val> (*)(App&)> const bins_list::map = _make_bins_list<bins_list::all>::map;
  char const* const* const bins_list::names = _make_bins_list<bins_list::all>::names;
  size_t const bins_list::count = _make_bins_list<bins_list::all>::count;


} // namespace sel
