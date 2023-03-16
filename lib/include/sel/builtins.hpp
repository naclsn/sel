#ifndef SEL_BUILTINS_HPP
#define SEL_BUILTINS_HPP

#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "engine.hpp"
#include "types.hpp"
#include "ll.hpp"
#include "unicode.hpp"

namespace sel {

  struct NumResult : Num {
  private:
    double n;

  public:
    NumResult(ref<Num> at, double n)
      : Num(at)
      , n(n)
    { }
    double value() override { return n; }
    ref<Val> copy() const override { return ref<NumResult>(h.app(), n); }

    double result() const { return n; }

  protected:
    VisitTable visit_table() const override {
      return make_visit_table<decltype(this)>::function();
    }
  };

  struct StrChunks : Str {
  private:
    typedef std::vector<std::string> ch_t;
    ch_t chs;
    ch_t::size_type at;

  public:
    StrChunks(ref<Str> at, std::vector<std::string> chunks)
      : Str(at, TyFlag::IS_FIN)
      , chs(chunks)
      , at(0)
    { }
    StrChunks(ref<Str> at, std::string single)
      : StrChunks(at, std::vector<std::string>({single}))
    { }
    std::ostream& stream(std::ostream& out) override {
      return out << chs[at++];
    }
    bool end() override {
      return chs.size() <= at;
    }
    std::ostream& entire(std::ostream& out) override {
      for (auto const& it : chs)
        out << it;
      return out;
    }
    ref<Val> copy() const override { return ref<StrChunks>(h.app(), chs); }

    std::vector<std::string> const& chunks() const { return chs; }

  protected:
    VisitTable visit_table() const override {
      return make_visit_table<decltype(this)>::function();
    }
  };

  /**
   * Seach for a value by name, return nullptr if not found.
   */
  ref<Val> lookup_name(App& app, std::string const& name);

  /**
   * Same as `lookup_name`, but at compile time. Argument
   * must be an identifier token.
   */
// #define static_lookup_name(__appref, __name) (ref<sel::bins::__name##_::Head>(__appref))
#define static_lookup_name(__appref, __name) ((ref<Fun>)ref<NumResult>(__appref, 0.))

  /**
   * namespace with types used to help constructing builtins
   */
  namespace bins_helpers {

    using namespace ll;

    // XXX: idk, just xxx (it sais 'not needed and not emitted', but it /is/ used..?)
    inline static Type done_applied(Type const& base_fty, Type const& ty) {
      Type res;
      base_fty.applied(ty, res);
      return res;
    }

    template <char c>
    struct unk : Val {
      typedef Val base_type;
      inline static Type make(char const* fname) {
        std::string vname(1, c);
        vname.push_back('_');
        vname.append(fname);
        return Type::makeUnk(std::move(vname));
      }
    };

    struct num : Num {
      typedef Num base_type;
      inline static Type make(char const* fname) {
        return Type::makeNum();
      }
      num(ref<num> at, Type&&): Num(at) { }
    };

    template <bool is_inf>
    struct _str : Str {
      typedef Str base_type;
      inline static Type make(char const* fname) {
        return Type::makeStr(is_inf);
      }
      _str(ref<_str> at, Type&& ty): Str(at, ty.is_inf()) { }
    };
    typedef _str<false> str;
    typedef _str<true> istr;

    template <bool is_inf, bool is_tpl, typename ...has>
    struct _lst : Lst {
      typedef Lst base_type;
      inline static Type make(char const* fname) {
        return Type::makeLst({has::make(fname)...}, is_inf, is_tpl);
      }
      _lst(ref<_lst> at, Type&& ty): Lst(at, std::forward<Type>(ty)) { }
    };
    template <typename ...has> using lst = _lst<false, false, has...>;
    template <typename ...has> using ilst = _lst<true, false, has...>;
    template <typename ...has> using tpl = _lst<false, true, has...>;

    template <typename from, typename to>
    struct fun : Fun {
      typedef Fun base_type;
      inline static Type make(char const* fname) {
        return Type::makeFun(std::move(from::make(fname)), std::move(to::make(fname)));
      }
      fun(ref<fun> at, Type&& ty): Fun(at, std::forward<Type>(ty)) { }
    };

    /**
     * at any level should be available:
     * - Pack: the pack the whole chain was built from
     * - Tail: the implementing's base structure
     * - Impl: the implementing structure
     * - Head: the constructible entry point (ie "the function itself")
     * - Next: the next in chain, which the current one instenciates
     * - Base: the previous in chain, which instanciated the current one
     * - Params: the pack for the params
     * - Args: the pack for the args
     * - paramsN: the number of parameters (ie pending arguments)
     * - argsN: the number of arguments (ie assigned parameters)
     * - args(): the (accessor for the) tuple of actual arguments
     *
     * note that even if the typedef exists, it is `void` for:
     * - Tail::Next
     * - Head::Base
     * also Tail::Params is not the empty pack, but the (last) return type
     *
     * on top of this, the Tail presents:
     * - name: the function's name
     * - doc: the function's docstring
     */
    template <typename impl_, typename PackItselfParams, typename PackItselfargs> struct make_bin;

    // makes the std::tuple of refs from ll::pack
    template <typename PackItself> struct make_arg_tuple;
    template <typename ...Pack>
    struct make_arg_tuple<pack<Pack...>> {
      typedef std::tuple<ref<Pack>...> type;
    };
    template <typename PackItself> using arg_tuple = typename make_arg_tuple<PackItself>::type;

    // @thx: https://stackoverflow.com/a/7858971
    template <unsigned ...> struct seq { };
    template <unsigned N, unsigned ...S>
    struct gens : gens<N-1, N-1, S...> { };
    template <unsigned ...S>
    struct gens<0, S...> { typedef seq<S...> type; };
    // to use the tuple of args (eg: `copy_arg_tuple(arg_unpack<Args>(), _args)`)
    template <unsigned N> using arg_unpack = typename gens<N>::type;

    // deep-copy the arg tuple
    template <unsigned ...S, typename tuple>
    static inline tuple copy_arg_tuple(seq<S...>, tuple t) {
      return {std::get<S>(t)->copy()...};
    }

    // make a vector off the arg tuple
    template <unsigned ...S, typename tuple>
    static inline std::vector<ref<Val>> args_vector(seq<S...>, tuple t) {
      return {std::get<S>(t)...};
    }

    // makes a pack 'a, b, c..' into a type 'a -> b -> c..'
    template <typename PackItself> struct make_fun_from_pack;
    template <typename h, typename ...t>
    struct make_fun_from_pack<pack<h, t...>> {
      typedef fun<h, typename make_fun_from_pack<pack<t...>>::type> type;
    };
    template <typename o>
    struct make_fun_from_pack<pack<o>> {
      typedef o type;
    };
    template <typename PackItself> using ty_from_pack = typename make_fun_from_pack<PackItself>::type;

    // common for make_bin
    template <typename instanciable, typename fParams, typename rArgs>
    struct _make_bin_common : ty_from_pack<fParams> {
      typedef fParams Params;
      typedef typename reverse<rArgs>::type Args;
      typedef typename join<Args, Params>::type Pack;

      constexpr static unsigned paramsN = count<Params>::value;
      constexpr static unsigned argsN = count<Args>::value;

      _make_bin_common(ref<Val> at, Type&& ty, arg_tuple<Args> t)
        : ty_from_pack<fParams>(at, std::forward<Type>(ty))
        , _args(t)
      { }

      ref<Val> copy() const override {
        return ref<instanciable>(this->h.app(), Type(this->ty), copy_arg_tuple(arg_unpack<argsN>(), _args));
      }

      arg_tuple<Args> const& args() const { return _args; }
      std::vector<ref<Val>> const args_v() const { return args_vector(arg_unpack<argsN>(), _args); }

    protected:
      VisitTable visit_table() const override {
        return make_visit_table<instanciable>::function();
      }

      arg_tuple<Args> _args;

      // used in Head/Body
      template <typename Next, typename param_h>
      ref<Val> _call_operator_template(ref<Val> arg) {
        ref<Val> ok = coerse<param_h>(this->h.app(), arg, this->ty.from());
        auto copy = this->h;
        Next::make_at(this->h, this->ty, _args, ok);
        return copy; // this->h would be accessing into deleted object
      }
    };

    // Body
    template <typename impl_, typename Params, typename rArgs>
    struct make_bin : _make_bin_common<make_bin<impl_, Params, rArgs>, Params, rArgs> {
      typedef _make_bin_common<make_bin, Params, rArgs> super;
      using super::super;
      typedef head_tail<Params> _pht;
      typedef head_tail<rArgs> _aht;

      typedef impl_ Impl;
      typedef make_bin<impl_, typename _pht::tail, typename join<pack<typename _pht::head>, rArgs>::type> Next;
      typedef make_bin<impl_, typename join<pack<typename _aht::head>, Params>::type, typename _aht::tail> Base;
      typedef typename Next::Tail Tail;
      typedef make_bin<impl_, typename super::Pack, pack<>> Head;

      // Body/Tail offset ctor
      static inline void make_at(ref<Val> at, Type const& base_type, arg_tuple<typename Base::Args> base_args, ref<Val> arg) {
        new make_bin(at, done_applied(base_type, arg->type()), std::tuple_cat(base_args, std::make_tuple(arg)));
      }

      // Head/Body overrides
      ref<Val> operator()(ref<Val> arg) override {
        return super::template _call_operator_template<Next, typename _pht::head::base_type>(arg);
      }
    };

    // Head
    template <typename impl_, typename Params>
    struct make_bin<impl_, Params, pack<>> : _make_bin_common<make_bin<impl_, Params, pack<>>, Params, pack<>> {
      typedef _make_bin_common<make_bin, Params, pack<>> super;
      using super::super;
      typedef head_tail<Params> _pht;

      typedef impl_ Impl;
      typedef make_bin<impl_, typename _pht::tail, pack<typename _pht::head>> Next;
      typedef void Base;
      typedef typename Next::Tail Tail;
      typedef make_bin Head;

      // Head-only constructor
      make_bin(ref<Val> at): super(at, super::make(impl_::name), std::tuple<>()) { }

      // Head/Body overrides
      ref<Val> operator()(ref<Val> arg) override {
        return super::template _call_operator_template<Next, typename _pht::head::base_type>(arg);
      }
    };

    // Tail
    template <typename impl_, typename ret, typename rArgs>
    struct make_bin<impl_, pack<ret>, rArgs> : _make_bin_common<impl_, pack<ret>, rArgs> {
      typedef _make_bin_common<impl_, pack<ret>, rArgs> super;
      using super::super;
      typedef head_tail<rArgs> _aht;

      typedef impl_ Impl;
      typedef void Next;
      typedef make_bin<impl_, pack<typename _aht::head, ret>, typename _aht::tail> Base;
      typedef make_bin Tail;
      typedef make_bin<impl_, typename super::Pack, pack<>> Head;

      // Body/Tail offset ctor
      static inline void make_at(ref<Val> at, Type const& base_type, arg_tuple<typename Base::Args> base_args, ref<Val> arg) {
        new impl_(at, done_applied(base_type, arg->type()), std::tuple_cat(base_args, std::make_tuple(arg)));
      }
    };

    // Head, non-function values
    template <typename impl_, typename single>
    struct make_bin<impl_, pack<single>, pack<>> : _make_bin_common<impl_, pack<single>, pack<>> {
      typedef _make_bin_common<impl_, pack<single>, pack<>> super;
      using super::super;

      typedef impl_ Impl;
      typedef void Next;
      typedef void Base;
      typedef make_bin Tail;
      typedef impl_ Head;

      // Head-only constructor
      make_bin(ref<Val> at): super(at, super::make(impl_::name), std::tuple<>()) { }
    };

    template <typename impl_, typename PackItself>
    struct make_bin_ww {
      typedef typename make_bin<impl_, typename reverse<PackItself>::type, pack<>>::Tail type;
    };
    template <typename impl_, char c, typename last, typename ...rest>
    struct make_bin_ww<impl_, pack<unk<c>, last, rest...>> {
      typedef typename make_bin<impl_, typename reverse<pack<fun<last, unk<c>>, rest...>>::type, pack<>>::Tail type;
    };

    template <typename impl_, typename ...types>
    using make_bin_w = typename make_bin_ww<impl_, typename reverse<pack<types...>>::type>::type;

  } // namespace bins_helpers

#define _BIN_num \
      double value() override;
#define _BIN_str \
      std::ostream& stream(std::ostream& out) override; \
      bool end() override; \
      std::ostream& entire(std::ostream& out) override;
#define _BIN_lst \
      ref<Val> operator*() override; \
      Lst& operator++() override; \
      bool end() override;
#define _BIN_unk \
      ref<Val> operator()(ref<Val>) override;

// used to remove the parenthesis from `__decl` and `__body`
#define __rem_par(...) __VA_ARGS__

// YYY: C-pp cannot do case operations in stringify,
// so to still have no conflict with the 65+ kw of C++,
// strategy is `lower_` (eg. `add_`, has `::name = "add"`)
// (couldn't get it with a constexpr tolower either...)
// SEE: https://stackoverflow.com/a/4225302
#define BIN(__ident, __decl, __docstr, __body) \
    struct __ident##_ : bins_helpers::make_bin_w<__ident##_, __rem_par __decl> { \
      constexpr static char const* name = #__ident; \
      constexpr static char const* doc = __docstr; \
      using Tail::Tail; \
      __rem_par __body \
    }

// YYY: could not find a reliable way to infer base solely on _d
// because `macro(templt<a, b>)` has 2 arguments...
#define BIN_num(_i, _d, _s, _b) BIN(_i, _d, _s, (_BIN_num; __rem_par _b))
#define BIN_str(_i, _d, _s, _b) BIN(_i, _d, _s, (_BIN_str; __rem_par _b))
#define BIN_lst(_i, _d, _s, _b) BIN(_i, _d, _s, (_BIN_lst; __rem_par _b))
#define BIN_unk(_i, _d, _s, _b) BIN(_i, _d, _s, (_BIN_unk; __rem_par _b))

  /**
   * namespace containing the actual builtins
   */
  namespace bins {

    using bins_helpers::unk;
    using bins_helpers::num;
    using bins_helpers::str;
    using bins_helpers::istr;
    using bins_helpers::lst;
    using bins_helpers::ilst;
    using bins_helpers::tpl;
    using bins_helpers::fun;

    BIN_num(add, (num, num, num),
      "add two numbers", ());

    BIN_lst(codepoints, (istr, ilst<num>),
      "split a string of bytes into its Unicode codepoints", (
      bool did_once = false;
      Str_istream sis;
      std::istream_iterator<codepoint> isi;
    ));

    BIN_num(div, (num, num, num),
      "divide the first number by the second number", ());

    BIN_unk(flip, (fun<unk<'a'>, fun<unk<'b'>, unk<'c'>>>, unk<'b'>, unk<'a'>, unk<'c'>),
      "flip the two parameters by passing the first given after the second one", ());

    BIN_lst(graphemes, (istr, ilst<str>),
      "split a stream of bytes into its Unicode graphemes", (
      bool did_once = false;
      Str_istream sis;
      std::istream_iterator<codepoint> isi;
      grapheme curr;
      bool past_end = false;
    ));

    BIN_unk(index, (ilst<unk<'a'>>, num, unk<'a'>),
      "select the value at the given index in the list (despite being called index it is an offset, ie. 0-based)", (
      bool did = false;
      ref<Val> found = ref<Val>(h.app(), nullptr);
    ));

    BIN_str(join, (str, ilst<istr>, istr),
      "join a list of string with a separator between entries", (
      std::string ssep;
      bool beginning = true;
    ));

    BIN_num(mul, (num, num, num),
      "multiply two numbers", ());

    BIN_num(sub, (num, num, num),
      "substract the second number from the first", ());

    BIN_num(tonum, (istr, num),
      "convert a string into number", (
      double r;
      bool done = false;
    ));

    BIN_str(tostr, (num, str),
      "convert a number into string", (
      bool read = false;
    ));

  } // namespace bins

  /**
   * namespace with the types `bins` and `bins_all` which lists
   *  - every builtin `Tail` types in alphabetical order
   *  - every builtin types (inc. intermediary) (not in any order)
   */
  namespace bins_ll {

    using namespace ll;
    using namespace bins;

    // YYY: (these are used in parsing - eg. coersion /
    // operators) could have these only here, but would
    // need to merge with below while keeping sorted (not
    // strictly necessary, but convenient); although for
    // now `bins_min` is not used
    typedef pack
      < add_
      , codepoints_
      , div_
      , flip_
      , graphemes_
      , index_
      , join_
      , mul_
      , sub_
      , tonum_
      , tostr_
      > bins_min;

    // XXX: still would love if this list could be built automatically
    typedef pack
      // < abs_
      // , add_
      // , bin_
      // , bytes_
      // , chr_
      // , codepoints_
      // , conjunction_
      // , const_
      // , contains_
      // , count_
      // , div_
      // , drop_
      // , dropwhile_
      // , duple_
      // , endswith_
      // , filter_
      // , flip_
      // , give_
      // , graphemes_
      // , head_
      // , hex_
      // , id_
      // , if_
      // , index_
      // , init_
      // , iterate_
      // , join_
      // , last_
      // , ln_
      // , map_
      // , mul_
      // , oct_
      // , ord_
      // , pi_
      // , prefix_
      // , repeat_
      // , replicate_
      // , reverse_
      // , singleton_
      // , split_
      // , startswith_
      // , sub_
      // , suffix_
      // , surround_
      // , tail_
      // , take_
      // , takewhile_
      // , tonum_
      // , tostr_
      // , tuple_
      // , unbin_
      // , unbytes_
      // , uncodepoints_
      // , uncurry_
      // , unhex_
      // , unoct_
      // , zipwith_
      <
      > bins_max;

// TODO: this is waiting for better answers (see also
// test_each).  Idea is: building with the whole `bins_max`
// ll is too heavy for quick dev-build (eg. when testing
// something out). The minimal set is `bins_min`, want a
// way to say what gets added on top of it (eg. just bytes,
// map, ln and give).
// I think it could be interesting to have a ll merge-sorted.
// #ifdef BINS_MIN
#if 1
    typedef bins_min bins;
#else
    typedef bins_max bins;
#endif

    namespace {
      constexpr bool is_sorted(char const* a, char const* b) {
        return *a < *b || (*a == *b && (*a == '\0' || is_sorted(a + 1, b + 1)));
      }

      template <typename PackItself> struct are_sorted;

      template <typename H1, typename H2, typename ...T>
      struct are_sorted<pack<H1, H2, T...>> {
        static constexpr bool function() {
          return is_sorted(H1::name, H2::name) && are_sorted<pack<H2, T...>>::function();
        }
      };
      template <typename O>
      struct are_sorted<pack<O>> {
        static constexpr bool function() {
          return true;
        }
      };
      template <>
      struct are_sorted<pack<>> {
        static constexpr bool function() {
          return true;
        }
      };

      static_assert(are_sorted<bins>::function(), "must be sorted");
    }

    // // make a chain from the types that makes the bin (ie Head to Tail)
    // template <typename Bin>
    // struct _make_bins_chain_pack {
    //   typedef typename join<pack<Bin>, typename _make_bins_chain_pack<typename Bin::Base>::type>::type type;
    // };
    // template <typename Next, typename last_to, typename last_from>
    // struct _make_bins_chain_pack<bins_helpers::_bin_be<Next, cons<last_to, cons<last_from, nil>>>> {
    //   typedef pack<bins_helpers::_bin_be<Next, cons<last_to, cons<last_from, nil>>>> type;
    // };
    // template <typename Impl>
    // struct _make_bins_chain_pack<bins_helpers::_fake_bin_be<Impl>> {
    //   typedef pack<> type;
    // };

    // // make a 2-dimensional pack of pack with the parts of the bins
    // template <typename PackItself> struct _make_bins_pack_of_chains;
    // template <typename ...Bins>
    // struct _make_bins_pack_of_chains<pack<Bins...>> {
    //   typedef pack<typename _make_bins_chain_pack<Bins>::type...> type;
    // };

    // typedef _make_bins_pack_of_chains<bins>::type bins_pack_of_chains;
    // typedef flatten<bins_pack_of_chains>::type bins_all;

  } // namespace bins_ll


  struct bins_list {
    static std::unordered_map<std::string, ref<Val> (*)(App&)> const map;
    static char const* const* const names;
    static size_t const count;
  };

} // namespace sel

#endif // SEL_BUILTINS_HPP
