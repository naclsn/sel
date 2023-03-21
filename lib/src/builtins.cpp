#include <cmath>

#include "sel/builtins.hpp"
#include "sel/parser.hpp"
#include "sel/utils.hpp"
#include "sel/visitors.hpp"

using namespace std;

namespace sel {

  using namespace packs;
  using namespace bins_helpers;

  unique_ptr<Val> lookup_name(string const& name) {
    auto itr = bins_list::map.find(name);
    if (bins_list::map.end() == itr) return nullptr;
    auto f = (*itr).second;
    return f();
  }

  template <typename U>
  static inline unique_ptr<U> clone(unique_ptr<U>& a)
  { return val_cast<U>(a->copy()); }

  template <typename U>
  static inline double eval(unique_ptr<U> n)
  { return n->value(); }

  // TODO: experiment with reworking Str's interface (eg. could return
  // a string_view-like rather than streaming, etc..)  note to self:
  // individual chunks of a Str could be string_views and that would
  // essentially resemble the ptr-len used in codegen; the main
  // underlying idea is that a not point is the caller supposed to
  // mutate returned chunk; now that also means that the lifespan
  // of said chunk is coupled with the Str instance in a somewhat
  // complex way: requesting a new chunk *invalidates* any previous
  // one (which, for comparison, is not the case with Lst items)
  // --
  // can make the C++-side type system work with me too because it
  // then become impossible to return an infinit string when requesting
  // a single chunk (behavior which would prevent eg. split_ from
  // operating correctly)
  // --
  // should it be possible to send an empty chunk? probably yes,
  // but then every consumer need to be aware and need to check for
  // that case (but then the check is on the consumer, ie-also. "you
  // pay for what you use" or whatever)
  // --
  // on the other hand, returning such as string_view means the Str
  // instance may need to hold any temporary (think 'tostr', 'oct'
  // or 'hex')

  // returns a finit complete string, which is not possible with a _str<is_inf=true>
  static inline string collect(unique_ptr<str> s) {
    ostringstream r;
    s->entire(r);
    return r.str();
  }

  template <bool is_inf, bool is_tpl, typename I>
  static inline unique_ptr<I> next(unique_ptr<_lst<is_inf, is_tpl, I>>& l)
  { return val_cast<I>(++*l); }
  template <unsigned n, bool is_inf, bool is_tpl, typename ...I>
  static inline unique_ptr<typename pack_nth<n, pack<I...>>::type> next(unique_ptr<_lst<is_inf, is_tpl, I...>>& l)
  { return val_cast<typename pack_nth<n, pack<I...>>::type>(++*l); }

  template <typename P, typename R>
  static inline unique_ptr<R> call(unique_ptr<fun<P, R>> f, unique_ptr<P> p)
  { return val_cast<R>((*f)(val_cast<Val>(move(p)))); }

#define _bind_some(__count) _bind_some_ ## __count
#define _bind_some_0()
#define _bind_some_1(a)          _bind_one(a, 0)
#define _bind_some_2(a, b)       _bind_one(a, 1); _bind_some_1(b)
#define _bind_some_3(a, b, c)    _bind_one(a, 2); _bind_some_2(b, c)
#define _bind_some_4(a, b, c, d) _bind_one(a, 3); _bind_some_3(b, c, d)

#define _bind_count(__count, ...) _bind_some(__count)(__VA_ARGS__)
#define _bind_one(__name, __depth) auto& __name = get<argsN-1 - __depth>(_args); (void)__name
// YYY: could it somehow be moved into `BIN_...`? in a way
// that it is only written once and the named arg refs
// are available all throughout the struct
#define bind_args(...) _bind_count(__VA_COUNT(__VA_ARGS__), __VA_ARGS__)

// XXX: this more temporary, but will keep for as long as flem
template <typename PackItself> struct _get_uarg;
template <typename P, typename R>
struct _get_uarg<pack<fun<P, R>>> { typedef P type; };
#define bind_uarg(__last) auto __last = val_cast<_get_uarg<Params>::type>(move(_##__last))
#define bind_uargs(__last, ...) bind_args(__VA_ARGS__); bind_uarg(__last)

  namespace bins {

    double abs_::value() {
      bind_args(n);
      return abs(eval(move(n)));
    }

    double add_::value() {
      bind_args(a, b);
      return eval(move(a)) + eval(move(b));
    }

    ostream& bin_::stream(ostream& out) {
      bind_args(n);
      uint64_t a = eval(move(n));
      char b[64+1]; b[64] = '\0';
      char* head = b+64;
      do { *--head = '0' + (a & 1); } while (a>>= 1);
      return out << head;
    }
    bool bin_::end() {
      bind_args(n);
      return !n;
    }
    ostream& bin_::entire(ostream& out) { return out << *this; }

    // void conjunction_::once() {
    //   bind_args(l, r);
    //   while (!l.end()) {
    //     ostringstream oss;
    //     ((handle<Str>)*l)->entire(oss);
    //     inleft.insert(oss.str());
    //     ++l;
    //   }
    //   while (!r.end()) {
    //     ostringstream oss;
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
    //     ostringstream oss;
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

    unique_ptr<Val> bytes_::operator++() {
      if (finished) return nullptr;
      bind_args(s);
      if (buff.length() == off) {
        if (s->end()) {
          s.reset();
          finished = true;
          return nullptr;
        }
        ostringstream oss;
        buff = (oss << *s, oss.str());
        off = 0;
      }
      return make_unique<NumResult>((uint8_t)buff[off++]);
    }

    ostream& chr_::stream(ostream& out) {
      bind_args(n);
      return out << codepoint(eval(move(n)));
    }
    bool chr_::end() {
      bind_args(n);
      return !n;
    }
    ostream& chr_::entire(ostream& out) {
      bind_args(n);
      return out << codepoint(eval(move(n)));
    }

    unique_ptr<Val> codepoints_::operator++() {
      static istream_iterator<codepoint> eos;
      if (eos == isi) return nullptr;
      double r = isi->u;
      ++isi;
      return make_unique<NumResult>(r);
    }

    unique_ptr<Val> const_::operator()(unique_ptr<Val> ignore) {
      bind_args(take);
      return move(take);
    }

    double contains_::value() {
      bind_args(substr, str);
      string ss = collect(move(substr));
      size_t sslen = ss.length();
      bool does = false;
      ostringstream oss;
      while (!does && !str->end()) { // YYY: same as endswith_, this could do with a `sslen`-long circular buffer
        size_t plen = oss.str().length();
        oss << *str;
        size_t len = oss.str().length();
        if (sslen <= len) { // have enough that searching is relevant
          string s = oss.str();
          for (auto k = plen < sslen ? sslen : plen; k <= len && !does; k++) { // only test over the range that was added
            does = 0 == s.compare(k-sslen, sslen, ss);
          }
        }
      }
      str.reset();
      return does;
    }

    double count_::value() {
      bind_args(matchit, l);
      string const search = collect(val_cast<str>(move(matchit))); // XXX
      size_t n = 0;
      while (auto it = next(l)) {
        string item = collect(val_cast<str>(move(it))); // XXX
        n+= search == item;
      }
      l.reset();
      return n;
    }

    double div_::value() {
      bind_args(a, b);
      return eval(move(a)) / eval(move(b));
    }

    unique_ptr<Val> drop_::operator++() {
      bind_args(n, l);
      if (n) {
        size_t k = 0;
        size_t const m = eval(move(n));
        while (auto it = next(l))
          if (m == k++) return it;
      }
      return next(l);
    }

    unique_ptr<Val> dropwhile_::operator++() {
      bind_args(p, l);
      if (!done) {
        done = true;
        while (auto it = next(l)) {
          if (!eval(call(clone(p), clone(it)))) {
            p.reset();
            return it;
          }
        }
      }
      return next(l);
    }

    unique_ptr<Val> duple_::operator++() {
      bind_args(o);
      if (o) {
        if (first) {
          first = false;
          return clone(o);
        }
        return move(o);
      }
      return nullptr;
    }

    double endswith_::value() {
      bind_args(suffix, str);
      string const sx = collect(move(suffix));
      size_t sxlen = sx.length();
      ostringstream oss;
      while (!str->end()) { // YYY: that's essentially '::entire', but endswith_ could leverage a circular buffer...
        oss << *str;
      }
      str.reset();
      string s = oss.str();
      return sxlen <= s.length() && 0 == s.compare(s.length()-sxlen, sxlen, sx);
    }

    unique_ptr<Val> filter_::operator++() {
      bind_args(p, l);
      while (auto it = next(l)) {
        if (eval(call(clone(p), clone(it))))
          return it;
      }
      p.reset();
      l.reset();
      return nullptr;
    }

    unique_ptr<Val> flip_::operator()(unique_ptr<Val> _a) {
      bind_uargs(a, fun, b);
      return call(call(move(fun), move(a)), move(b));
    }

    unique_ptr<Val> give_::operator++() {
      bind_args(n, l);
      if (!l) return nullptr;
      if (n) {
        size_t const count = eval(move(n));
        if (0 < count) {
          circ.reserve(count);
          bool full = false;
          while (auto it = next(l)) {
            circ.emplace_back(move(it));
            // buffer filled
            if ((full = circ.size() == count)) break;
          }
          if (!full) {
            // here if the list was smaller than the count
            l.reset();
            return nullptr;
          }
        }
      }
      // 'give 0'
      if (circ.empty()) return next(l);
      if (circ.size() == at) at = 0;
      auto r = next(l);
      if (!r) {
        l.reset();
        for (auto& it : circ) it.reset();
        return nullptr;
      }
      swap(circ[at++], r);
      return r;
    }

    unique_ptr<Val> graphemes_::operator++() {
      static istream_iterator<codepoint> const eos;
      if (eos == isi) return nullptr;
      grapheme curr;
      read_grapheme(isi, curr);
      ostringstream oss;
      return unique_ptr<Val>(new StrChunks((oss << curr, oss.str())));
    }

    unique_ptr<Val> head_::operator()(unique_ptr<Val> _l) {
      bind_uarg(l);
      auto it = next(l);
      if (!it) throw RuntimeError("head of empty list");
      l.reset();
      return it;
    }

    ostream& hex_::stream(ostream& out) {
      bind_args(n);
      return out << hex << (size_t)eval(move(n));
    }
    bool hex_::end() {
      bind_args(n);
      return !n;
    }
    ostream& hex_::entire(ostream& out) {
      bind_args(n);
      return out << hex << (size_t)eval(move(n));
    }

    unique_ptr<Val> id_::operator()(unique_ptr<Val> take) {
      return take;
    }

    unique_ptr<Val> if_::operator()(unique_ptr<Val> _argument) {
      bind_uargs(argument, condition, consequence, alternative);
      unique_ptr<Num> n = call(move(condition), move(argument));
      bool is = eval(move(n));
      return move(is ? consequence : alternative);
    }

    unique_ptr<Val> index_::operator()(unique_ptr<Val> _k) {
      bind_uargs(k, l);
      size_t len = 0;
      size_t const idx = eval(move(k));
      while (auto it = next(l))
        if (idx == len++)
          return it;
      ostringstream oss;
      throw RuntimeError((oss << "index out of range: " << idx << " but length is " << len, oss.str()));
    }

    unique_ptr<Val> init_::operator++() {
      bind_args(l);
      if (!l) return nullptr;
      unique_ptr<Val> r = move(prev);
      prev = next(l);
      if (!r) {
        if (!prev) throw RuntimeError("init of empty list");
        r = move(prev);
        prev = next(l);
      }
      if (!prev) {
        l.reset();
        return nullptr;
      }
      return r;
    }

    unique_ptr<Val> iterate_::operator++() {
      bind_args(f, o);
      auto r = call(clone(f), clone(o));
      swap(o, r);
      return r;
    }

    ostream& join_::stream(ostream& out) {
      bind_args(sep, lst);
      if (!curr) {
        curr = next(lst);
        if (!curr) {
          lst.reset();
          return out;
        }
      }
      out << *curr;
      if (curr->end()) {
        curr.reset();
        if (sep) ssep = collect(move(sep));
        out << ssep;
      }
      return out;
    }
    bool join_::end() {
      bind_args(sep, lst);
      return !lst;
    }
    ostream& join_::entire(ostream& out) {
      bind_args(sep, lst);
      auto first = next(lst);
      if (!first) return out;
      first->entire(out);
      ssep = collect(move(sep));
      while (auto it = next(lst)) {
        out << ssep;
        it->entire(out);
      }
      return out;
    }

    unique_ptr<Val> last_::operator()(unique_ptr<Val> _l) {
      bind_uarg(l);
      auto prev = next(l);
      if (!prev) throw RuntimeError("last of empty list");
      while (auto it = next(l)) {
        prev = move(it);
      }
      return prev;
    }

    unique_ptr<Val> map_::operator++() {
      bind_args(f, l);
      auto it = next(l);
      return it ? call(clone(f), move(it)) : nullptr;
    }

    double mul_::value() {
      bind_args(a, b);
      return eval(move(a)) * eval(move(b));
    }

    ostream& ln_::stream(ostream& out) {
      bind_args(s);
      return !s->end() ? out << *s : (s.reset(), out << '\n');
    }
    bool ln_::end() {
      bind_args(s);
      return !s;
    }
    ostream& ln_::entire(ostream& out) {
      bind_args(s);
      return s->entire(out) << '\n';
    }

    double pi_::value() {
      return M_PI;
    }

    ostream& oct_::stream(ostream& out) {
      bind_args(n);
      return out << oct << (size_t)eval(move(n));
    }
    bool oct_::end() {
      bind_args(n);
      return !n;
    }
    ostream& oct_::entire(ostream& out) {
      bind_args(n);
      return out << oct << (size_t)eval(move(n));
    }

    double ord_::value() {
      bind_args(s);
      codepoint c;
      Str_streambuf sb(move(s));
      istream(&sb) >> c;
      return c.u;
    }

    ostream& prefix_::stream(ostream& out) {
      bind_args(s, px);
      if (px) {
        out << *px;
        if (px->end()) px.reset();
      } else if (s) {
        out << *s;
        if (s->end()) s.reset();
      }
      return out;
    }
    bool prefix_::end() {
      bind_args(px, s);
      return !s;
    }
    ostream& prefix_::entire(ostream& out) {
      bind_args(px, s);
      px->entire(out);
      px.reset();
      s->entire(out);
      s.reset();
      return out;
    }

    unique_ptr<Val> repeat_::operator++() {
      bind_args(o);
      return clone(o);
    }

    unique_ptr<Val> replicate_::operator++() {
      bind_args(n, o);
      if (!o) return nullptr;
      if (n) todo = eval(move(n));
      if (0 == todo--) {
        o.reset();
        return nullptr;
      }
      return clone(o);
    }

    unique_ptr<Val> reverse_::operator++() {
      bind_args(l);
      if (l) {
        while (auto it = next(l))
          cache.emplace_back(move(it));
        curr = cache.size();
        l.reset();
      }
      if (0 == curr) return nullptr;
      return move(cache[--curr]);
    }

    unique_ptr<Val> singleton_::operator++() {
      bind_args(o);
      return o ? move(o) : nullptr;
    }

    // XXX: idk if that could be re-visited
    void split_::once() {
      bind_args(sep, str);
      ostringstream oss;
      sep->entire(oss);
      sep.reset();
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
      string buf = acc.str();
      string::size_type at = buf.find(ssep);
      if (string::npos != at) {
        // found in current acc, pop(0)
        curr = buf.substr(0, at);
        acc = ostringstream(buf.substr(at+ssep.size()));
        return;
      }
      if (str->end()) {
        // send the rest of acc, set end
        curr = buf;
        at_end = true;
        str.reset();
        return;
      }
      acc << *str;
      return next();
    }
    unique_ptr<Val> split_::operator++() {
      next();
      if (at_past_end) return nullptr;
      return unique_ptr<Val>(new StrChunks(curr));
    }

    double startswith_::value() {
      bind_args(prefix, str);
      string px = collect(move(prefix));
      ostringstream oss;
      while (oss.str().length() < px.length() && 0 == px.compare(0, oss.str().length(), oss.str()) && !str->end()) {
        oss << *str;
      }
      str.reset();
      return 0 == oss.str().compare(0, px.length(), px);
    }

    double sub_::value() {
      bind_args(a, b);
      return eval(move(a)) - eval(move(b));
    }

    ostream& suffix_::stream(ostream& out) {
      bind_args(sx, s);
      if (s) {
        out << *s;
        if (s->end()) s.reset();
      } else if (sx) {
        out << *sx;
        if (sx->end()) sx.reset();
      }
      return out;
    }
    bool suffix_::end() {
      bind_args(sx, s);
      return !sx;
    }
    ostream& suffix_::entire(ostream& out) {
      bind_args(sx, s);
      s->entire(out);
      s.reset();
      sx->entire(out);
      sx.reset();
      return out;
    }

    ostream& surround_::stream(ostream& out) {
      bind_args(px, sx, s);
      if (px) {
        out << *px;
        if (px->end()) px.reset();
      } else if (s) {
        out << *s;
        if (s->end()) s.reset();
      } else if (sx) {
        out << *sx;
        if (sx->end()) sx.reset();
      }
      return out;
    }
    bool surround_::end() {
      bind_args(px, sx, s);
      return !sx;
    }
    ostream& surround_::entire(ostream& out) {
      bind_args(px, sx, s);
      px->entire(out);
      px.reset();
      s->entire(out);
      s.reset();
      sx->entire(out);
      sx.reset();
      return out;
    }

    unique_ptr<Val> tail_::operator++() {
      bind_args(l);
      if (!done) {
        done = true;
        if (!next(l)) throw RuntimeError("tail of empty list");
      }
      return next(l);
    }

    unique_ptr<Val> take_::operator++() {
      bind_args(n, l);
      if (!l) return nullptr;
      if (n) todo = eval(move(n));
      if (0 == todo--) {
        l.reset();
        return nullptr;
      }
      return next(l);
    }

    unique_ptr<Val> takewhile_::operator++() {
      bind_args(p, l);
      if (!l) return nullptr;
      auto it = next(l);
      if (!it) {
        p.reset();
        l.reset();
        return nullptr;
      }
      if (eval(call(clone(p), clone(it)))) return it;
      p.reset();
      l.reset();
      return nullptr;
    }

    double tonum_::value() {
      bind_args(s);
      Str_streambuf sb(move(s));
      double r;
      istream(&sb) >> r;
      return r;
    }

    ostream& tostr_::stream(ostream& out) {
      bind_args(n);
      return out << eval(move(n));
    }
    bool tostr_::end() {
      bind_args(n);
      return !n;
    }
    ostream& tostr_::entire(ostream& out) {
      bind_args(n);
      return out << eval(move(n));
    }

    unique_ptr<Val> tuple_::operator++() {
      bind_args(a, b);
      if (a) return move(a);
      if (b) return move(b);
      return nullptr;
    }

    double unbin_::value() {
      bind_args(s);
      size_t n = 0;
      while (!s->end()) {
        ostringstream oss;
        string buff = (oss << *s, oss.str());
        for (char const c : buff) {
          if ('0' != c && '1' != c) goto out;
          n = (n << 1) | (c - '0');
        }
      }
    out:
      s.reset();
      return n;
    }

    ostream& unbytes_::stream(ostream& out) {
      bind_args(l);
      if (auto it = next(l))
        out << (char)eval(move(it));
      else l.reset();
      return out;
    }
    bool unbytes_::end() {
      bind_args(l);
      return !l;
    }
    ostream& unbytes_::entire(ostream& out) {
      bind_args(l);
      while (auto it = next(l))
        out << (char)eval(move(it));
      l.reset();
      return out;
    }

    ostream& uncodepoints_::stream(ostream& out) {
      bind_args(l);
      if (auto it = next(l))
        out << (codepoint)eval(move(it));
      else l.reset();
      return out;
    }
    bool uncodepoints_::end() {
      bind_args(l);
      return !l;
    }
    ostream& uncodepoints_::entire(ostream& out) {
      bind_args(l);
      while (auto it = next(l))
        out << (codepoint)eval(move(it));
      l.reset();
      return out;
    }

    unique_ptr<Val> uncurry_::operator()(unique_ptr<Val> _pair) {
      bind_uargs(pair, f);
      auto a = next<0>(pair);
      auto b = next<1>(pair);
      return call(call(move(f), move(a)), move(b));
    }

    double unhex_::value() {
      bind_args(s);
      size_t n = 0;
      Str_streambuf sb(move(s));
      istream(&sb) >> hex >> n;
      return n;
    }

    double unoct_::value() {
      bind_args(s);
      size_t n = 0;
      Str_streambuf sb(move(s));
      istream(&sb) >> oct >> n;
      return n;
    }

    unique_ptr<Val> zipwith_::operator++() {
      bind_args(f, l1, l2);
      auto a = next(l1);
      auto b = next(l2);
      return a && b ? call(call(clone(f), move(a)), move(b)) : nullptr;
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
  struct _make_bins_list<pack<Pack...>> {
    static unordered_map<string, unique_ptr<Val> (*)()> const map;
    static char const* const names[];
    constexpr static size_t count = sizeof...(Pack);
  };

  template <typename ...Pack>
  constexpr char const* const _make_bins_list<pack<Pack...>>::names[] = {Pack::name...};

  template <typename Va>
  unique_ptr<Val> _bin_new() { return make_unique<typename Va::Head>(); }
  // XXX: static constructor
  template <typename ...Pack>
  unordered_map<string, unique_ptr<Val> (*)()> const _make_bins_list<pack<Pack...>>::map = {{Pack::name, _bin_new<Pack>}...};

  unordered_map<string, unique_ptr<Val> (*)()> const bins_list::map = _make_bins_list<bins_list::all>::map;
  char const* const* const bins_list::names = _make_bins_list<bins_list::all>::names;
  size_t const bins_list::count = _make_bins_list<bins_list::all>::count;


} // namespace sel
