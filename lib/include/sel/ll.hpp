#ifndef SEL_LL_HPP
#define SEL_LL_HPP

namespace sel {

  /**
   * namespace for TMP linked list
   * YYY: is the `cons` mess necessary, again?
   */
  namespace ll {

    // /**
    //  * empty list, see `ll::cons`
    //  */
    // struct nil;

    // /**
    //  * linked lists of type (car,cdr) style, see `ll::nil`
    //  */
    // template <typename A, typename D>
    // struct cons { typedef A car; typedef D cdr; };

    // /**
    //  * make a linked lists of types (car,cdr) style from pack (see `ll::cons`)
    //  */
    // template <typename H, typename... T>
    // struct cons_l { typedef cons<H, typename cons_l<T...>::type> type; };
    // template <typename O>
    // struct cons_l<O> { typedef cons<O, nil> type; };

    // template <typename from, typename into> struct _rev_impl;
    // /**
    //  * reverse a list of types
    //  */
    // template <typename list>
    // struct rev { typedef typename _rev_impl<list, nil>::type type; };
    // template <typename into>
    // struct _rev_impl<nil, into> { typedef into type; };
    // template <typename H, typename T, typename into>
    // struct _rev_impl<cons<H, T>, into> { typedef typename _rev_impl<T, cons<H, into>>::type type; };

    // /**
    //  * count the number of element
    //  */
    // template <typename list> struct count;
    // template <typename head, typename tail>
    // struct count<cons<head, tail>> { static constexpr unsigned value = count<tail>::value+1; };
    // template <typename only>
    // struct count<cons<only, nil>> { static constexpr unsigned value = 1; };

    /**
     * manipulate a list of types through a pack, eg.:
     * ```cpp
     * template <typename PackItself> struct some;
     * template <typename ...Pack> struct some<pack<...Pack>> { }; // use Pack...
     * ```
     */
    template <typename ...T> struct pack;

    // this to help transition from ll::cons
    // /**
    //  * makes a `ll::pack` from an `ll::cons`
    //  */
    // template <typename L, typename ...Pack>
    // struct packer {
    //   typedef typename packer<typename L::cdr, Pack..., typename L::car>::type type;
    // };
    // template <typename ...Pack>
    // struct packer<nil, Pack...> {
    //   typedef pack<Pack...> type;
    // };

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

    template <typename PackItselfFrom, typename PackItselfInto> struct _reverse_impl;
    /**
     * reverse a ll::pack
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
     * join multiple ll::pack sequentially into a single one
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

    // /**
    //  * flatten a pack of packs XXX: aint that the same a join?
    //  */
    // template <typename PackItself> struct flatten;
    // template <typename ...Pack>
    // struct flatten<pack<Pack...>> {
    //   typedef typename join<Pack...>::type type;
    // };

  } // namespace ll

} // namespace sel

#endif // SEL_LL_HPP
