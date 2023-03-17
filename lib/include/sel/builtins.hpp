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
    NumResult(handle<Num> at, double n)
      : Num(at)
      , n(n)
    { }
    double value() override { return n; }
    handle<Val> copy() const override { return handle<NumResult>(h.app(), n); }

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
    StrChunks(handle<Str> at, std::vector<std::string> chunks)
      : Str(at, TyFlag::IS_FIN)
      , chs(chunks)
      , at(0)
    { }
    StrChunks(handle<Str> at, std::string single)
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
    handle<Val> copy() const override { return handle<StrChunks>(h.app(), chs); }

    std::vector<std::string> const& chunks() const { return chs; }

  protected:
    VisitTable visit_table() const override {
      return make_visit_table<decltype(this)>::function();
    }
  };

  /**
   * Seach for a value by name, return nullptr if not found.
   */
  handle<Val> lookup_name(App& app, std::string const& name);

  /**
   * Same as `lookup_name`, but at compile time. Argument
   * must be an identifier token.
   */
#define static_lookup_name(__appref, __name) (sel::App::handle<sel::bins::__name##_::Head>(__appref))

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
      num(handle<num> at, Type&&): Num(at) { }
    };

    template <bool is_inf>
    struct _str : Str {
      typedef Str base_type;
      inline static Type make(char const* fname) {
        return Type::makeStr(is_inf);
      }
      _str(handle<_str> at, Type&& ty): Str(at, ty.is_inf()) { }
    };
    typedef _str<false> str;
    typedef _str<true> istr;

    template <bool is_inf, bool is_tpl, typename ...has>
    struct _lst : Lst {
      typedef Lst base_type;
      inline static Type make(char const* fname) {
        return Type::makeLst({has::make(fname)...}, is_inf, is_tpl);
      }
      _lst(handle<_lst> at, Type&& ty): Lst(at, std::forward<Type>(ty)) { }
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
      fun(handle<fun> at, Type&& ty): Fun(at, std::forward<Type>(ty)) { }
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
      typedef std::tuple<handle<Pack>...> type;
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
    static inline std::vector<handle<Val>> args_vector(seq<S...>, tuple t) {
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

      _make_bin_common(handle<Val> at, Type&& ty, arg_tuple<Args> t)
        : ty_from_pack<fParams>(at, std::forward<Type>(ty))
        , _args(t)
      { }

      handle<Val> copy() const override {
        return handle<instanciable>(this->h.app(), Type(this->ty), copy_arg_tuple(arg_unpack<argsN>(), _args));
      }

      arg_tuple<Args> const& args() const { return _args; }
      std::vector<handle<Val>> const args_v() const { return args_vector(arg_unpack<argsN>(), _args); }

    protected:
      VisitTable visit_table() const override {
        return make_visit_table<instanciable>::function();
      }

      arg_tuple<Args> _args;

      // used in Head/Body
      template <typename Next, typename param_h>
      handle<Val> _call_operator_template(handle<Val> arg) {
        handle<Val> ok = coerse<param_h>(this->h.app(), arg, this->ty.from());
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
      static inline void make_at(handle<Val> at, Type const& base_type, arg_tuple<typename Base::Args> base_args, handle<Val> arg) {
        new make_bin(at, done_applied(base_type, arg->type()), std::tuple_cat(base_args, std::make_tuple(arg)));
      }

      // Head/Body overrides
      handle<Val> operator()(handle<Val> arg) override {
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
      make_bin(handle<Val> at): super(at, super::make(impl_::name), std::tuple<>()) { }

      // Head/Body overrides
      handle<Val> operator()(handle<Val> arg) override {
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
      static inline void make_at(handle<Val> at, Type const& base_type, arg_tuple<typename Base::Args> base_args, handle<Val> arg) {
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
      make_bin(handle<Val> at): super(at, super::make(impl_::name), std::tuple<>()) { }
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
      handle<Val> operator*() override; \
      Lst& operator++() override; \
      bool end() override;
#define _BIN_unk \
      handle<Val> operator()(handle<Val>) override;

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

    BIN_num(abs, (num, num),
      "return the absolute value of a number", ());

    BIN_num(add, (num, num, num),
      "add two numbers", ());

    BIN_str(bin, (num, str),
      "convert a number to its binary textual representation (without leading '0b')", (
      bool read = false;
    ));

    BIN_lst(bytes, (istr, ilst<num>),
      "split a string of bytes into its actual bytes as numbers", (
      std::string buff;
      std::string::size_type off = std::string::npos;
    ));

    BIN_str(chr, (num, str),
      "make a string of a single character with the given unicode codepoint", (
      bool read = false;
    ));

    BIN_lst(codepoints, (istr, ilst<num>),
      "split a string of bytes into its Unicode codepoints", (
      bool did_once = false;
      Str_istream sis;
      std::istream_iterator<codepoint> isi;
    ));

    BIN_lst(conjunction, (lst<unk<'a'>>, ilst<unk<'a'>>, ilst<unk<'a'>>),
      "logical conjunction between two lists treated as sets; it is right-lazy and the list order is taken from the right argument (for now items are expected to be strings, until arbitraty value comparison)", (
      bool did_once = false;
      std::unordered_set<std::string> inleft;
      void once();
    ));

    BIN_unk(const, (unk<'a'>, unk<'b'>, unk<'a'>),
      "always evaluate to its first argument, ignoring its second argument", ());

    BIN_num(contains, (str, istr, num),
      "true if the string contain the given substring", (
      bool done = false, does;
    ));

    BIN_num(count, (unk<'a'>, lst<unk<'a'>>, num),
      "count occurrences of an item in a sequence (for now items are expected to be strings, until arbitraty value comparison)", (
      bool done = false;
      unsigned n;
    ));

    BIN_num(div, (num, num, num),
      "divide the first number by the second number", ());

    BIN_lst(drop, (num, ilst<unk<'a'>>, ilst<unk<'a'>>),
      "return the suffix past a given count, or the empty list if it is shorter", (
      bool done = false;
    ));

    BIN_lst(dropwhile, (fun<unk<'a'>, num>, ilst<unk<'a'>>, ilst<unk<'a'>>),
      "return the suffix remaining from the first element not verifying the predicate onward", (
      bool done = false;
    ));

    BIN_lst(duple, (unk<'a'>, tpl<unk<'a'>, unk<'a'>>),
      "create a pair off of a single item", (
      int did = 0;
    ));

    BIN_num(endswith, (str, str, num),
      "true if the string ends with the given suffix", (
      bool done = false, does;
    ));

    BIN_lst(filter, (fun<unk<'a'>, num>, ilst<unk<'a'>>, ilst<unk<'a'>>),
      "return the list of elements which satisfy the predicate", (
      handle<Val> curr = handle<Val>(h.app(), nullptr);
    ));

    BIN_unk(flip, (fun<unk<'a'>, fun<unk<'b'>, unk<'c'>>>, unk<'b'>, unk<'a'>, unk<'c'>),
      "flip the two parameters by passing the first given after the second one", ());

    BIN_lst(give, (num, lst<unk<'a'>>, lst<unk<'a'>>),
      "return the prefix prior to the given count, or the empty list if it is shorter", (
      bool did_once = false;
      size_t at = 0;
      size_t at_when_end = 0;
      std::vector<handle<Val>> circ;
      void once();
    ));

    BIN_lst(graphemes, (istr, ilst<str>),
      "split a stream of bytes into its Unicode graphemes", (
      bool did_once = false;
      Str_istream sis;
      std::istream_iterator<codepoint> isi;
      grapheme curr;
      bool past_end = false;
    ));

    BIN_unk(head, (ilst<unk<'a'>>, unk<'a'>),
      "extract the first element of a list, which must not be empty", ());

    BIN_str(hex, (num, str),
      "convert a number to its hexadecimal textual representation (without leading '0x')", (
      bool read = false;
    ));

    BIN_unk(id, (unk<'a'>, unk<'a'>),
      "the identity function, returns its input", ());

    BIN_unk(if, (fun<unk<'a'>, num>, unk<'b'>, unk<'b'>, unk<'a'>, unk<'b'>),
      "take a condition, a consequence and an alternative; return consequence if the argument verifies the condition, alternative otherwise", ());

    BIN_unk(index, (ilst<unk<'a'>>, num, unk<'a'>),
      "select the value at the given index in the list (despite being called index it is an offset, ie. 0-based)", (
      bool did = false;
      handle<Val> found = handle<Val>(h.app(), nullptr);
    ));

    BIN_lst(init, (lst<unk<'a'>>, lst<unk<'a'>>),
      "extract the elements before the last one of a list, which must not be empty", (
      handle<Val> prev = handle<Val>(h.app(), nullptr);
    ));

    BIN_lst(iterate, (fun<unk<'a'>, unk<'a'>>, unk<'a'>, ilst<unk<'a'>>),
      "return an infinite list of repeated applications of the function to the input", (
      handle<Val> curr = handle<Val>(h.app(), nullptr);
    ));

    BIN_str(join, (str, ilst<istr>, istr),
      "join a list of string with a separator between entries", (
      std::string ssep;
      bool beginning = true;
    ));

    BIN_unk(last, (lst<unk<'a'>>, unk<'a'>),
      "extract the last element of a list, which must not be empty", ());

    BIN_str(ln, (istr, istr),
      "append a new line to a string", (
      bool done = false;
    ));

    BIN_lst(map, (fun<unk<'a'>, unk<'b'>>, ilst<unk<'a'>>, ilst<unk<'b'>>),
      "make a new list by applying an unary operation to each value from a list", ());

    BIN_num(mul, (num, num, num),
      "multiply two numbers", ());

    BIN_str(oct, (num, str),
      "convert a number to its octal textual representation (without leading '0o')", (
      bool read = false;
    ));

    BIN_num(ord, (istr, num),
      "give the codepoint of the (first) character", ());

    BIN_num(pi, (num),
      "pi, what did you expect", ());

    BIN_str(prefix, (str, istr, istr),
      "prepend a prefix to a string", ());

    BIN_lst(repeat, (unk<'a'>, ilst<unk<'a'>>),
      "repeat an infinite amount of copies of the same value", ());

    BIN_lst(replicate, (num, unk<'a'>, lst<unk<'a'>>),
      "replicate a finite amount of copies of the same value", (
      size_t did = 0;
    ));

    BIN_lst(reverse, (lst<unk<'a'>>, lst<unk<'a'>>),
      "reverse the order of the elements in the list", (
      std::vector<handle<Val>> cache;
      bool did_once = false;
      size_t curr;
      void once();
    ));

    BIN_lst(singleton, (unk<'a'>, lst<unk<'a'>>),
      "make a list of a single item", (
      bool done = false;
    ));

    BIN_lst(split, (str, istr, ilst<istr>),
      "break a string into pieces separated by the argument, consuming the delimiter; note that an empty delimiter does not split the input on every character, but rather is equivalent to `const [repeat ::]`, for this see `graphemes`", (
      bool did_once = false;
      std::string ssep;
      std::ostringstream acc = std::ostringstream(std::ios_base::ate);
      std::string curr;
      bool at_end = false;
      bool at_past_end = false;
      bool init = false;
      void once();
      void next();
    ));

    BIN_num(startswith, (str, istr, num),
      "true if the string starts with the given prefix", (
      bool done = false, does;
    ));

    BIN_num(sub, (num, num, num),
      "substract the second number from the first", ());

    BIN_str(suffix, (istr, str, istr),
      "append a suffix to a string", ());

    BIN_str(surround, (str, str, str, str),
      "surround a string with a prefix and a suffix", ());

    BIN_lst(tail, (ilst<unk<'a'>>, ilst<unk<'a'>>),
      "extract the elements after the first one of a list, which must not be empty", (
      bool done = false;
    ));

    BIN_lst(take, (num, ilst<unk<'a'>>, lst<unk<'a'>>),
      "return the prefix of a given length, or the entire list if it is shorter", (
      size_t did = 0;
    ));

    BIN_lst(takewhile, (fun<unk<'a'>, num>, ilst<unk<'a'>>, ilst<unk<'a'>>),
      "return the longest prefix of elements statisfying the predicate", ());

    BIN_num(tonum, (istr, num),
      "convert a string into number", (
      double r;
      bool done = false;
    ));

    BIN_str(tostr, (num, str),
      "convert a number into string", (
      bool read = false;
    ));

    BIN_lst(tuple, (unk<'a'>, unk<'b'>, tpl<unk<'a'>, unk<'b'>>),
      "construct a tuple", (
      int did = 0;
    ));

    BIN_num(unbin, (istr, num),
      "convert a number into string from its binary textual representation (without leading '0b')", (
      double r;
      bool done = false;
    ));

    BIN_str(unbytes, (ilst<num>, istr),
      "construct a string from its actual bytes; this can lead to broken UTF-8 or 'degenerate cases' if not careful", ());

    BIN_str(uncodepoints, (ilst<num>, istr),
      "construct a string from its Unicode codepoints; this can lead to 'degenerate cases' if not careful", ());

    BIN_unk(uncurry, (fun<unk<'a'>, fun<unk<'b'>, unk<'c'>>>, tpl<unk<'a'>, unk<'b'>>, unk<'c'>),
      "convert a curried function to a function on pairs", ());

    BIN_num(unhex, (istr, num),
      "convert a number into string from its hexadecimal textual representation (without leading '0x')", (
      double r;
      bool done = false;
    ));

    BIN_num(unoct, (istr, num),
      "convert a number into string from its octal textual representation (without leading '0o')", (
      double r;
      bool done = false;
    ));

    BIN_lst(zipwith, (fun<unk<'a'>, fun<unk<'b'>, unk<'c'>>>, ilst<unk<'a'>>, ilst<unk<'b'>>, ilst<unk<'c'>>),
      "make a new list by applying an binary operation to each corresponding value from each lists; stops when either list ends", (
      handle<Val> curr = handle<Val>(h.app(), nullptr);
    ));

  } // namespace bins

  struct bins_list {
    // YYY: (these are used in parsing - eg. coersion /
    // operators) could have these only here, but would
    // need to merge with below while keeping sorted (not
    // strictly necessary, but convenient); although for
    // now `bins_min` is not used
    typedef ll::pack
      < bins::add_
      , bins::codepoints_
      , bins::div_
      , bins::flip_
      , bins::graphemes_
      , bins::index_
      , bins::join_
      , bins::mul_
      , bins::sub_
      , bins::tonum_
      , bins::tostr_
      > min;

    // XXX: still would love if this list could be built automatically
    // (any chances to leverage templates if `bins` is made a class..?)
    typedef ll::pack
      < bins::abs_
      , bins::add_
      , bins::bin_
      , bins::bytes_
      , bins::chr_
      , bins::codepoints_
      , bins::conjunction_
      , bins::const_
      , bins::contains_
      , bins::count_
      , bins::div_
      , bins::drop_
      , bins::dropwhile_
      , bins::duple_
      , bins::endswith_
      , bins::filter_
      , bins::flip_
      , bins::give_
      , bins::graphemes_
      , bins::head_
      , bins::hex_
      , bins::id_
      , bins::if_
      , bins::index_
      , bins::init_
      , bins::iterate_
      , bins::join_
      , bins::last_
      , bins::ln_
      , bins::map_
      , bins::mul_
      , bins::oct_
      , bins::ord_
      , bins::pi_
      , bins::prefix_
      , bins::repeat_
      , bins::replicate_
      , bins::reverse_
      , bins::singleton_
      , bins::split_
      , bins::startswith_
      , bins::sub_
      , bins::suffix_
      , bins::surround_
      , bins::tail_
      , bins::take_
      , bins::takewhile_
      , bins::tonum_
      , bins::tostr_
      , bins::tuple_
      , bins::unbin_
      , bins::unbytes_
      , bins::uncodepoints_
      , bins::uncurry_
      , bins::unhex_
      , bins::unoct_
      , bins::zipwith_
      > more;

// TODO: this is waiting for better answers (see also
// test_each).  Idea is: building with the whole `bins_max`
// ll is too heavy for quick dev-build (eg. when testing
// something out). The minimal set is `bins_min`, want a
// way to say what gets added on top of it (eg. just bytes,
// map, ln and give).
// I think it could be interesting to have a ll merge-sorted.
#ifdef BINS_MIN
    typedef min all;
#else
    typedef more all;
#endif

  private:
    static inline constexpr bool is_sorted(char const* a, char const* b) {
      return *a < *b || (*a == *b && (*a == '\0' || is_sorted(a + 1, b + 1)));
    }

    template <typename PackItself>
    struct are_sorted { };
    template <typename H1, typename H2, typename ...T>
    struct are_sorted<ll::pack<H1, H2, T...>> : are_sorted<ll::pack<H2, T...>> {
      static_assert(is_sorted(H1::name, H2::name), "must be sorted");
    };

    static inline void static_assert_sorted() { are_sorted<all>{}; }

  public:
    static std::unordered_map<std::string, handle<Val> (*)(App&)> const map;
    static char const* const* const names;
    static size_t const count;
  }; // bins_list

} // namespace sel

#endif // SEL_BUILTINS_HPP
