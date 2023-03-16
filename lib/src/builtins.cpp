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

  namespace bins_helpers {

    // template <typename Impl, typename one>
    // ref<Val> _bin_be<Impl, ll::cons<one, ll::nil>>::the::copy() const {
    //   typedef _bin_be<Impl, ll::cons<one, ll::nil>>::the a;
    //   return ref<typename a::Base::Next>(this->h.app()); // copyOne
    // }

    // template <typename Impl, typename last_arg, char b>
    // ref<Val> _bin_be<Impl, cons<fun<last_arg, unk<b>>, nil>>::the::copy() const {
    //   typedef _bin_be<Impl, cons<fun<last_arg, unk<b>>, nil>>::the a;
    //   return ref<typename a::Base::Next>(this->h.app()); // copyOne2
    // }

    // template <typename NextT, typename to, typename from, typename from_again, typename from_more>
    // ref<Val> _bin_be<NextT, ll::cons<to, ll::cons<from, ll::cons<from_again, from_more>>>>::copy() const {
    //   typedef _bin_be<NextT, ll::cons<to, ll::cons<from, ll::cons<from_again, from_more>>>> a;
    //   return ref<a>(
    //     this->h.app(),
    //     _base->copy(),
    //     _arg->copy()
    //   ); // copyBody
    // }

    // template <typename NextT, typename last_to, typename last_from>
    // ref<Val> _bin_be<NextT, ll::cons<last_to, ll::cons<last_from, ll::nil>>>::_the_when_not_unk::copy() const {
    //   typedef _bin_be<NextT, ll::cons<last_to, ll::cons<last_from, ll::nil>>>::the a;
    //   return ref<typename a::Base::Next>(
    //     this->h.app(),
    //     _base->copy(),
    //     _arg->copy()
    //   ); // copyTail1
    // }
    // template <typename NextT, typename last_to, typename last_from>
    // ref<Val> _bin_be<NextT, ll::cons<last_to, ll::cons<last_from, ll::nil>>>::_the_when_is_unk::copy() const {
    //   typedef _bin_be<NextT, ll::cons<last_to, ll::cons<last_from, ll::nil>>>::the a;
    //   return ref<typename a::Base::Next>(
    //     this->h.app(),
    //     _base->copy(),
    //     _arg->copy()
    //   ); // copyTail2
    // }

    // template <typename NextT, typename last_to, typename last_from>
    // ref<Val> _bin_be<NextT, ll::cons<last_to, ll::cons<last_from, ll::nil>>>::copy() const {
    //   typedef _bin_be<NextT, ll::cons<last_to, ll::cons<last_from, ll::nil>>> a;
    //   return ref<a>(this->h.app()); // copyHead
    // }

  } // namespace bins_helpers

#define _depth(__depth) _depth_ ## __depth
#define _depth_0 _arg
#define _depth_1 _base->_depth_0
#define _depth_2 _base->_depth_1
#define _depth_3 _base->_depth_2

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

    ;

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

  namespace bins {
    constexpr char const* const add_::name;
    constexpr char const* const pi_::name;
    constexpr char const* const tonum_::name;
  }

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
