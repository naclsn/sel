#ifndef SEL_UTILS_HPP
#define SEL_UTILS_HPP

#include <iostream>
#include <memory>

// @thx: https://renenyffenegger.ch/notes/development/languages/C-C-plus-plus/preprocessor/macros/__VA_ARGS__/count-arguments
#define __A11(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, ...) a11
#define __VA_COUNT(...) __A11(dum, ## __VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define TRACE(...) // std::cerr << __VA_ARGS__ << "\n"

// YYY: because this exists in C++14 onward
namespace std {
  template <typename T, typename ...Args>
  unique_ptr<T> make_unique(Args&&... args) {
    return unique_ptr<T>(new T(std::forward<Args>(args)...));
  }
}

namespace sel {

  template <typename To, typename From>
  inline std::unique_ptr<To> val_cast(std::unique_ptr<From> from) {
    return std::unique_ptr<To>((To*)from.release());
  }

  struct Val;
  namespace utils {

    /**
     * For use in printing a pointer.
     */
    struct raw {
      void const* ptr;
      raw(void const* ptr): ptr(ptr) { }
    };
    std::ostream& operator<<(std::ostream& out, raw ptr);

    /**
     * For use in representing a value.
     */
    struct repr {
      Val const& val;
      // XXX: copy-pasted from visitors, that's boring but whatever
      // TODO: better, that temporary
      bool single_line = true; // result on a single line
      bool no_recurse = false; // do not allow recursing into fields
      bool only_class = false; // just output the class name, no recursing, no whitespaces
      inline repr(Val const& val): val(val) { }
      inline repr(Val const& val, bool single_line, bool no_recurse, bool only_class)
        : val(val)
        , single_line(single_line)
        , no_recurse(no_recurse)
        , only_class(only_class)
      { }
    };
    std::ostream& operator<<(std::ostream& out, repr me);

    /**
     * For use in representing text.
     */
    struct quoted {
      std::string const& str;
      bool do_col;
      bool put_quo;
      inline quoted(std::string const& str): str(str), do_col(false), put_quo(true) { }
      inline quoted(std::string const& str, bool do_col, bool put_quo): str(str), do_col(do_col), put_quo(put_quo) { }
    };
    std::ostream& operator<<(std::ostream& out, quoted q);

  } // namespace utils

  namespace packs {

    /**
     * manipulate a list of types through a pack, eg.:
     * ```cpp
     * template <typename PackItself> struct some;
     * template <typename ...Pack> struct some<pack<...Pack>> { }; // use Pack...
     * ```
     */
    template <typename ...T> struct pack;

    template <typename PackItself> struct count;
    template <typename ...Pack>
    struct count<pack<Pack...>> {
      constexpr static unsigned value = sizeof...(Pack);
    };

    template <typename Searched, unsigned cur, typename Head, typename ...Tail>
    struct _packed_index { static constexpr unsigned value = _packed_index<Searched, cur+1, Tail...>::value; };
    template <typename Searched, unsigned cur, typename ...Rest>
    struct _packed_index<Searched, cur, Searched, Rest...> { static constexpr unsigned value = cur; };
    /**
     * get the index of a type in the pack (it must be in, and it matches only on the first one)
     */
    template <typename Searched, typename PackItself> struct pack_index;
    template <typename Searched, typename ...Pack>
    struct pack_index<Searched, pack<Pack...>> {
      static constexpr unsigned value = _packed_index<Searched, 0, Pack...>::value;
    };

    /**
     * get the type at index
     */
    template <unsigned N, typename PackItself> struct pack_nth;
    template <unsigned N, typename h, typename ...t>
    struct pack_nth<N, pack<h, t...>> {
      typedef typename pack_nth<N-1, pack<t...>>::type type;
    };
    template <typename h, typename ...t>
    struct pack_nth<0, pack<h, t...>> {
      typedef h type;
    };

    template <typename PackItselfFrom, typename PackItselfInto> struct _reverse_impl;
    /**
     * reverse a pack
     */
    template <typename PackItself>
    struct reverse {
      typedef typename _reverse_impl<PackItself, pack<>>::type type;
    };
    template <typename PackItselfInto>
    struct _reverse_impl<pack<>, PackItselfInto> {
      typedef PackItselfInto type;
    };
    template <typename Head, typename ...PackFrom, typename ...PackInto>
    struct _reverse_impl<pack<Head, PackFrom...>, pack<PackInto...>> {
      typedef typename _reverse_impl<pack<PackFrom...>, pack<Head, PackInto...>>::type type;
    };

    /**
     * join multiple pack sequentially into a single one
     */
    template <typename ...PacksThemselves> struct join;
    template <>
    struct join<> {
      typedef pack<> type;
    };
    template <typename ...Pack>
    struct join<pack<Pack...>> {
      typedef pack<Pack...> type;
    };
    template <typename ...PackA, typename ...PackB, typename ...PacksThemselves>
    struct join<pack<PackA...>, pack<PackB...>, PacksThemselves...> {
      typedef typename join<pack<PackA..., PackB...>, PacksThemselves...>::type type;
    };

    /**
     * extract into ::head and ::tail
     */
    template <typename PackItself> struct head_tail;
    template <typename h, typename ...t>
    struct head_tail<pack<h, t...>> {
      typedef h head;
      typedef pack<t...> tail;
    };

  } // namespace packs

} // namespace sel

#endif // SEL_UTILS_HPP
