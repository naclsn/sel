#ifndef SEL_LL_HPP
#define SEL_LL_HPP

namespace sel {

  /**
   * namespace for TMP linked list
   * YYY: is the `cons` mess necessary, again?
   */
  namespace ll {

    /**
     * empty list, see `ll::cons`
     */
    struct nil;

    /**
     * linked lists of type (car,cdr) style, see `ll::nil`
     */
    template <typename A, typename D>
    struct cons { typedef A car; typedef D cdr; };

    /**
     * make a linked lists of types (car,cdr) style from pack (see `ll::cons`)
     */
    template <typename H, typename... T>
    struct cons_l { typedef cons<H, typename cons_l<T...>::type> type; };
    template <typename O>
    struct cons_l<O> { typedef cons<O, nil> type; };

    template <typename from, typename into> struct _rev_impl;
    /**
     * reverse a list of types
     */
    template <typename list>
    struct rev { typedef typename _rev_impl<list, nil>::type type; };
    template <typename into>
    struct _rev_impl<nil, into> { typedef into type; };
    template <typename H, typename T, typename into>
    struct _rev_impl<cons<H, T>, into> { typedef typename _rev_impl<T, cons<H, into>>::type type; };

    /**
     * count the number of element
     */
    template <typename list> struct count;
    template <typename head, typename tail>
    struct count<cons<head, tail>> { static constexpr unsigned value = count<tail>::value+1; };
    template <typename only>
    struct count<cons<only, nil>> { static constexpr unsigned value = 1; };

    /**
     * manipulate a cons list through a pack, eg.:
     * ```cpp
     * template <typename PackItself> struct some;
     * template <typename ...Pack> struct some<pack<...Pack>> { }; // use Pack...
     * ```
     */
    template <typename ...T> struct pack;

    /**
     * makes a `ll::pack` from an `ll::cons`
     */
    template <typename L, typename ...Pack>
    struct packer {
      typedef typename packer<typename L::cdr, Pack..., typename L::car>::packed packed;
    };
    template <typename ...Pack>
    struct packer<nil, Pack...> {
      typedef pack<Pack...> packed;
    };

    template <typename Searched, unsigned cur, typename Head, typename ...Tail>
    struct _packed_index { static constexpr unsigned value = _packed_index<Searched, cur+1, Tail...>::value; };
    template <typename Searched, unsigned cur, typename ...Rest>
    struct _packed_index<Searched, cur, Searched, Rest...> { static constexpr unsigned value = cur; };

    template <typename Searched, typename PackItself> struct pack_index;
    template <typename Searched, typename ...Pack>
    struct pack_index<Searched, pack<Pack...>> {
      static constexpr unsigned value = _packed_index<Searched, 0, Pack...>::value;
    };

  } // namespace ll

} // namespace sel

#endif // SEL_LL_HPP
