#include <cmath>

#include "sel/builtins.hpp"
#include "sel/parser.hpp"
#include "sel/utils.hpp"
#include "sel/visitors.hpp"

namespace sel {

  ref<Val> lookup_name(App& app, std::string const& name) {
    auto itr = bins_list::map.find(name);
    if (bins_list::map.end() == itr) return ref<Val>(app, nullptr);
    auto f = (*itr).second;
    return f(app);
  }

  // ZZZ: idk
  template <typename U>
  static inline ref<U> clone(U& a) { return a.copy(); }

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
      return std::abs(std::get<0>(_args)->value());
    }

    double add_::value() {
      bind_args(a, b);
      return a.value() + b.value();
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

    void conjunction_::once() {
      bind_args(l, r);
      while (!l.end()) {
        std::ostringstream oss;
        ((ref<Str>)*l)->entire(oss);
        inleft.insert(oss.str());
        ++l;
      }
      while (!r.end()) {
        std::ostringstream oss;
        ((ref<Str>)*r)->entire(oss);
        if (inleft.end() != inleft.find(oss.str())) break;
        ++r;
      }
      did_once = true;
    }
    ref<Val> conjunction_::operator*() {
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
        ((ref<Str>)*r)->entire(oss);
        if (inleft.end() != inleft.find(oss.str())) break;
        ++r;
      }
      return *this;
    }
    bool conjunction_::end() {
      bind_args(l, r);
      return (did_once ? inleft.empty() : l.end()) || r.end();
    }

    ref<Val> bytes_::operator*() {
      bind_args(s);
      if (buff.length() <= off && !s.end()) {
        std::ostringstream oss;
        buff = (oss << s, oss.str());
        off = 0;
      }
      return ref<NumResult>(h.app(), (uint8_t)buff[off]);
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
    bool bytes_::end() {
      bind_args(s);
      return s.end() && buff.length() <= off;
    }

    std::ostream& chr_::stream(std::ostream& out) { read = true; return out << codepoint(std::get<0>(_args)->value()); }
    bool chr_::end() { return read; }
    std::ostream& chr_::entire(std::ostream& out) { read = true; return out << codepoint(std::get<0>(_args)->value()); }

    ref<Val> codepoints_::operator*() {
      bind_args(s);
      if (!did_once) {
        isi = std::istream_iterator<codepoint>(sis = Str_istream(&s));
        did_once = true;
      }
      return ref<NumResult>(h.app(), isi->u);
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
    bool codepoints_::end() {
      bind_args(s);
      static std::istream_iterator<codepoint> eos;
      return did_once ? eos == isi : s.end();
    }

    ref<Val> const_::operator()(ref<Val> ignore) {
      bind_args(take);
      return &take;
    }

    double contains_::value() {
      if (!done) {
        bind_args(substr, str);
        std::ostringstream ossss;
        substr.entire(ossss);
        std::string ss = ossss.str();
        auto sslen = ss.length();
        does = false;
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
        done = true;
      }
      return does;
    }

    double count_::value() {
      if (!done) {
        bind_args(it, l);
        std::ostringstream search; ((Str&)it).entire(search); // XXX
        n = 0;
        for (; !l.end(); ++l) {
          std::ostringstream item; ((ref<Str>)*l)->entire(item); // XXX
          if (search.str() == item.str())
            n++;
        }
        done = true;
      }
      return n;
    }

    double div_::value() {
      bind_args(a, b);
      return a.value() / b.value();
    }

    ref<Val> drop_::operator*() {
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
    bool drop_::end() {
      bind_args(n, l);
      if (done) return l.end();
      size_t k;
      for (k = 0; k < n.value() && !l.end(); k++)
        ++l;
      done = true;
      return l.end() && n.value() != k;
    }

    ref<Val> dropwhile_::operator*() {
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
    bool dropwhile_::end() {
      bind_args(p, l);
      if (done) return l.end();
      while (!l.end() && p(*l))
        ++l;
      done = true;
      return l.end();
    }

    ref<Val> duple_::operator*() {
      bind_args(v);
      return v.copy();
    }
    Lst& duple_::operator++() {
      ++did;
      return *this;
    }
    bool duple_::end() {
      return 2 == did;
    }

    double endswith_::value() {
      if (!done) {
        bind_args(suffix, str);
        std::ostringstream osssx;
        suffix.entire(osssx);
        std::string sx = osssx.str();
        auto sxlen = sx.length();
        std::ostringstream oss;
        while (!str.end()) { // YYY: that's essentially '::entire', but endswith_ could leverage a circular buffer...
          oss << str;
        }
        std::string s = oss.str();
        does = sxlen <= s.length() && 0 == s.compare(s.length()-sxlen, sxlen, sx);
        done = true;
      }
      return does;
    }

    ref<Val> filter_::operator*() {
      bind_args(p, l);
      if (!curr) {
        while (!l.end() && !((ref<Num>)p(*l))->value()) ++l;
        curr = *l;
      }
      return *l;
    }
    Lst& filter_::operator++() {
      bind_args(p, l);
      ++l;
      while (!l.end() && !((ref<Num>)p(*l))->value()) ++l;
      curr = *l;
      return *this;
    }
    bool filter_::end() {
      bind_args(p, l);
      while (!l.end() && !((ref<Num>)p(*l))->value()) ++l;
      return l.end();
    }

    ref<Val> flip_::operator()(ref<Val> a) {
      bind_args(fun, b);
      return (*(ref<Fun>)fun(a))(&b);
    }

    void give_::once() {
      bind_args(n, l);
      size_t count = n.value();
      circ.reserve(count);
      for (; !l.end() && circ.size() < count; ++l)
        circ.push_back(*l);
      if (l.end()) at_when_end = at;
      did_once = true;
    }
    ref<Val> give_::operator*() {
      bind_args(n, l);
      if (!did_once) once();
      return 0 != circ.size() ? circ[at] : *l;
    }
    Lst& give_::operator++() {
      bind_args(n, l);
      if (!did_once) once();
      if (0 != circ.size())
        circ[++at] = *l;
      ++l;
      if (l.end()) at_when_end = at;
      return *this;
    }
    bool give_::end() {
      bind_args(n, l);
      if (!did_once) once();
      return l.end() && at_when_end == at;
    }

    ref<Val> graphemes_::operator*() {
      bind_args(s);
      if (!did_once) {
        isi = std::istream_iterator<codepoint>(sis = Str_istream(&s));
        did_once = true;
        read_grapheme(isi, curr);
      }
      std::ostringstream oss;
      return ref<StrChunks>(h.app(), (oss << curr, oss.str()));
    }
    Lst& graphemes_::operator++() {
      bind_args(s);
      if (!did_once) {
        isi = std::istream_iterator<codepoint>(sis = Str_istream(&s));
        did_once = true;
        read_grapheme(isi, curr);
      }
      curr.clear();
      static std::istream_iterator<codepoint> const eos;
      if (eos == isi) past_end = true;
      else read_grapheme(isi, curr);
      return *this;
    }
    bool graphemes_::end() {
      bind_args(s);
      static std::istream_iterator<codepoint> const eos;
      return did_once ? eos == isi && past_end : s.end();
    }

    ref<Val> head_::operator()(ref<Val> _l) {
      auto& l = *(ref<Lst>)_l;
      if (l.end()) throw RuntimeError("head of empty list");
      return *l;
    }

    std::ostream& hex_::stream(std::ostream& out) { read = true; return out << std::hex << size_t(std::get<0>(_args)->value()); }
    bool hex_::end() { return read; }
    std::ostream& hex_::entire(std::ostream& out) { read = true; return out << std::hex << size_t(std::get<0>(_args)->value()); }

    ref<Val> id_::operator()(ref<Val> take) {
      return take;
    }

    ref<Val> if_::operator()(ref<Val> argument) {
      bind_args(condition, consequence, alternative);
      return ((ref<Num>)condition(argument))->value()
        ? &consequence
        : &alternative;
    }

    ref<Val> index_::operator()(ref<Val> _k) {
      if (!did) {
        bind_args(l); auto& k = *(ref<Num>)_k;
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

    ref<Val> init_::operator*() {
      bind_args(l);
      if (!prev) {
        if (l.end()) throw RuntimeError("init of empty list");
        prev = *l;
        ++l;
      }
      return prev;
    }
    Lst& init_::operator++() {
      bind_args(l);
      if (!prev) {
        if (l.end()) throw RuntimeError("init of empty list");
        ++l;
      }
      prev = *l;
      ++l;
      return *this;
    }
    bool init_::end() {
      bind_args(l);
      if (!prev) {
        if (l.end()) throw RuntimeError("init of empty list");
        prev = *l;
        ++l;
      }
      return l.end();
    }

    ref<Val> iterate_::operator*() {
      bind_args(f, o);
      return !curr ? &o : curr;
    }
    Lst& iterate_::operator++() {
      bind_args(f, o);
      curr = f(!curr ? &o : curr);
      return *this;
    }
    bool iterate_::end() { return false; }

    std::ostream& join_::stream(std::ostream& out) {
      bind_args(sep, lst);
      if (beginning) {
        std::ostringstream oss;
        sep.entire(oss);
        ssep = oss.str();
        beginning = false;
      } else out << ssep;
      ref<Str> it = *lst;
      it->entire(out);
      ++lst;
      return out;
    }
    bool join_::end() {
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
      ((ref<Str>)*lst)->entire(out);
      ++lst;
      for (; !lst.end(); ++lst) {
        ((ref<Str>)*lst)->entire(out << ssep);
      }
      return out;
    }

    ref<Val> last_::operator()(ref<Val> _l) {
      auto& l = *(ref<Lst>)_l;
      if (l.end()) throw RuntimeError("last of empty list");
      ref<Val> r(h.app(), nullptr);
      for (; !l.end(); ++l)
        r = *l;
      return r;
    }

    ref<Val> map_::operator*() {
      bind_args(f, l);
      return (*clone(f))(*l);
    }
    Lst& map_::operator++() {
      bind_args(f, l);
      ++l;
      return *this;
    }
    bool map_::end() {
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
      codepoint c;
      Str_istream(std::get<0>(_args)) >> c;
      return c.u;
    }

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

    ref<Val> repeat_::operator*() { return std::get<0>(_args)->copy(); }
    Lst& repeat_::operator++() { return *this; }
    bool repeat_::end() { return false; }

    ref<Val> replicate_::operator*() {
      if (!did) did++;
      bind_args(n, o);
      return o.copy();
    }
    Lst& replicate_::operator++() {
      did++;
      return *this;
    }
    bool replicate_::end() {
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
    ref<Val> reverse_::operator*() {
      if (!did_once) once();
      return cache[curr-1];
    }
    Lst& reverse_::operator++() {
      curr--;
      return *this;
    }
    bool reverse_::end() {
      if (did_once) return 0 == curr;
      bind_args(l);
      return l.end();
    }

    ref<Val> singleton_::operator*() { return std::get<0>(_args); }
    Lst& singleton_::operator++() { done = true; return *this; }
    bool singleton_::end() { return done; }

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
    ref<Val> split_::operator*() {
      if (!init) { next(); init = true; }
      return ref<StrChunks>(h.app(), curr);
    }
    Lst& split_::operator++() {
      if (!init) { next(); init = true; }
      next();
      return *this;
    }
    bool split_::end() {
      return at_past_end;
    }

    double startswith_::value() {
      if (!done) {
        bind_args(prefix, str);
        std::ostringstream osspx;
        prefix.entire(osspx);
        std::string px = osspx.str();
        std::ostringstream oss;
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

    ref<Val> tail_::operator*() {
      bind_args(l);
      if (!done) {
        if (l.end()) throw RuntimeError("tail of empty list");
        ++l;
        done = true;
      }
      return *l;
    }
    Lst& tail_::operator++() {
      bind_args(l);
      if (!done) {
        if (l.end()) throw RuntimeError("tail of empty list");
        ++l;
        done = true;
      }
      ++l;
      return *this;
    }
    bool tail_::end() {
      bind_args(l);
      if (!done) {
        if (l.end()) throw RuntimeError("tail of empty list");
        ++l;
        done = true;
      }
      return l.end();
    }

    ref<Val> take_::operator*() {
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
    bool take_::end() {
      bind_args(n, l);
      auto x = n.value();
      return 0 == x || x < did || l.end();
    }

    ref<Val> takewhile_::operator*() {
      bind_args(p, l);
      return *l;
    }
    Lst& takewhile_::operator++() {
      bind_args(p, l);
      ++l;
      return *this;
    }
    bool takewhile_::end() {
      bind_args(p, l);
      return l.end() || !((ref<Num>)p(*l))->value();
    }

    double tonum_::value() {
      if (!done) {
        bind_args(s);
        Str_istream(&s) >> r;
        done = true;
      }
      return r;
    }

    std::ostream& tostr_::stream(std::ostream& out) { read = true; return out << std::get<0>(_args)->value(); }
    bool tostr_::end() { return read; }
    std::ostream& tostr_::entire(std::ostream& out) { read = true; return out << std::get<0>(_args)->value(); }

    ref<Val> tuple_::operator*() {
      bind_args(a, b);
      return 0 == did ? &a : &b;
    }
    Lst& tuple_::operator++() {
      ++did;
      return *this;
    }
    bool tuple_::end() {
      return 2 == did;
    }

    double unbin_::value() {
      if (!done) {
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
        r = n;
        done = true;
      }
      return r;
    }

    std::ostream& unbytes_::stream(std::ostream& out) {
      bind_args(l);
      char b = ((ref<Num>)*l)->value();
      ++l;
      return out << b;
    }
    bool unbytes_::end() {
      bind_args(l);
      return l.end();
    }
    std::ostream& unbytes_::entire(std::ostream& out) {
      bind_args(l);
      for (; !l.end(); ++l)
        out << char(((ref<Num>)*l)->value());
      return out;
    }

    std::ostream& uncodepoints_::stream(std::ostream& out) {
      bind_args(l);
      codepoint cp = ((ref<Num>)*l)->value();
      ++l;
      return out << cp;
    }
    bool uncodepoints_::end() {
      bind_args(l);
      return l.end();
    }
    std::ostream& uncodepoints_::entire(std::ostream& out) {
      bind_args(l);
      for (; !l.end(); ++l)
        out << codepoint(((ref<Num>)*l)->value());
      return out;
    }

    ref<Val> uncurry_::operator()(ref<Val> _pair) {
      auto& pair = *(ref<Lst>)_pair;
      bind_args(f);
      return (*(ref<Fun>)f(*pair))(*++pair);
    }

    double unhex_::value() {
      if (!done) {
        bind_args(s);
        size_t n = 0;
        Str_istream(&s) >> std::hex >> n;
        r = n;
        done = true;
      }
      return r;
    }

    double unoct_::value() {
      if (!done) {
        bind_args(s);
        size_t n = 0;
        Str_istream(&s) >> std::oct >> n;
        r = n;
        done = true;
      }
      return r;
    }

    ref<Val> zipwith_::operator*() {
      bind_args(f, l1, l2);
      if (!curr) curr = (*(ref<Fun>)f(*l1))(*l2);
      return curr;
    }
    Lst& zipwith_::operator++() {
      bind_args(f, l1, l2);
      curr = ref<Val>(h.app(), nullptr);
      ++l1;
      ++l2;
      return *this;
    }
    bool zipwith_::end() {
      bind_args(f, l1, l2);
      return l1.end() || l2.end();
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
    static std::unordered_map<std::string, ref<Val> (*)(App&)> const map;
    static char const* const names[];
    constexpr static size_t count = sizeof...(Pack);
  };

  template <typename ...Pack>
  constexpr char const* const _make_bins_list<ll::pack<Pack...>>::names[] = {Pack::name...};

  template <typename Va>
  ref<Val> _bin_new(App& app) { return ref<typename Va::Head>(app); }
  // XXX: static constructor
  template <typename ...Pack>
  std::unordered_map<std::string, ref<Val> (*)(App&)> const _make_bins_list<ll::pack<Pack...>>::map = {{Pack::name, _bin_new<Pack>}...};

  std::unordered_map<std::string, ref<Val> (*)(App&)> const bins_list::map = _make_bins_list<bins_ll::bins>::map;
  char const* const* const bins_list::names = _make_bins_list<bins_ll::bins>::names;
  size_t const bins_list::count = _make_bins_list<bins_ll::bins>::count;


} // namespace sel
