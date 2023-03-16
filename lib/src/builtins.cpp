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

    double add_::value() {
      bind_args(a, b);
      return a.value() + b.value();
    }

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

    double div_::value() {
      bind_args(a, b);
      return a.value() / b.value();
    }

    ref<Val> flip_::operator()(ref<Val> a) {
      bind_args(fun, b);
      return (*(ref<Fun>)fun(a))(&b);
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

    ref<Val> index_::operator()(ref<Val> _k) {
      if (!did) {
        auto& k = *(ref<Num>)_k;
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

    double mul_::value() {
      bind_args(a, b);
      return a.value() * b.value();
    }

    double sub_::value() {
      bind_args(a, b);
      return a.value() - b.value();
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

  } // namespace bins

// YYY: somehow, this seems to only be needed with BINS_MIN
// #ifdef BINS_MIN
#if 1
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
