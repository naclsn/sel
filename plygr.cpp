#include <iostream>
#include <tuple>

#include "sel/engine.hpp"
#include "sel/ll.hpp"

namespace sel {

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
      inline static Type make(char const* fname) {
        std::string vname(1, c);
        vname.push_back('_');
        vname.append(fname);
        return Type::makeUnk(std::move(vname));
      }
    };

    struct num : Num {
      inline static Type make(char const* fname) {
        return Type::makeNum();
      }
      template <typename ...Args>
      num(ref<num> at, Args...)
        : Num(at)
      { }
    };

    template <bool is_inf>
    struct _str : Str {
      inline static Type make(char const* fname) {
        return Type::makeStr(is_inf);
      }
      template <typename ...Args>
      _str(ref<_str> at, Args...)
        : Str(at, TyFlag::IS_FIN)
      { }
    };
    typedef _str<false> str;
    typedef _str<true> istr;

    template <bool is_inf, bool is_tpl, typename ...has>
    struct _lst : Lst {
      inline static Type make(char const* fname) {
        return Type::makeLst({has::make(fname)...}, is_inf, is_tpl);
      }
      _lst(ref<_lst> at, char const* fname)
        : Lst(at, make(fname))
      { }
      _lst(ref<_lst> at, Type const& base_fty, Type const& ty)
        : Lst(at, done_applied(base_fty, ty))
      { }
    };
    template <typename ...has> using lst = _lst<false, false, has...>;
    template <typename ...has> using ilst = _lst<true, false, has...>;
    template <typename ...has> using tpl = _lst<false, true, has...>;

    template <typename from, typename to>
    struct fun : Fun {
      inline static Type make(char const* fname) {
        return Type::makeFun(std::move(from::make(fname)), std::move(to::make(fname)));
      }
      fun(ref<fun> at, char const* fname)
        : Fun(at, make(fname))
      { }
      fun(ref<fun> at, Type const& base_fty, Type const& ty)
        : Fun(at, done_applied(base_fty, ty))
      { }
    };

    /**
     * at any level should be available:
     * - Self: the current type (?)
     * - Pack: the pack the whole chain was built from
     * - Tail: the implementing structure
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
    // to use the tuple of args (eg: `{}`)
    template <unsigned N> using arg_unpack = typename gens<N>::type;

    template <unsigned ...S, typename tuple>
    static inline tuple copy_arg_tuple(seq<S...>, tuple t) {
      return {std::get<S>(t)->copy()...};
    }

    // Head
    template <typename impl_, typename param_h, typename ...params>
    struct make_bin<impl_, pack<param_h, params...>, pack<>> : fun<param_h, pack<params...>> {
      typedef make_bin Self;

      typedef pack<param_h, params...> Pack;
      typedef pack<param_h, params...> Params;
      typedef pack<> Args;

      constexpr static unsigned paramsN = count<Params>::value;
      constexpr static unsigned argsN = count<Args>::value;

      typedef make_bin<impl_, pack<params...>, pack<param_h>> Next;
      typedef void Base;

      typedef typename Next::Tail Tail;
      typedef Self Head;

      make_bin(ref<make_bin> at, arg_tuple<Args> t)
        : fun<param_h, pack<params...>>(at, impl_::name)
        , _args(t)
      { }

      ref<Val> operator()(ref<Val> arg) override {
        ref<param_h> ok = coerse<param_h>(this->h.app(), arg, this->type().from());
        new Next(this->h, this->type(), _args, ok);
        return this->h;
      }

      ref<Val> copy() const override {
        return ref<make_bin>(this->h.app(), copy_arg_tuple(arg_unpack<argsN>(), _args));
      }

    protected:
      VisitTable visit_table() const override {
        return make_visit_table<decltype(this)>::function();
      }

      arg_tuple<Args> _args;
    };

    // Body
    template <typename impl_, typename param_h, typename ...params, typename arg_h, typename ...args>
    struct make_bin<impl_, pack<param_h, params...>, pack<arg_h, args...>> : fun<param_h, pack<params...>> {
      typedef make_bin Self;

      typedef typename join<typename reverse<pack<args...>>::type, pack<arg_h, param_h, params...>>::type Pack;
      typedef pack<param_h, params...> Params;
      typedef typename reverse<pack<arg_h, args...>>::type Args;

      constexpr static unsigned paramsN = count<Params>::value;
      constexpr static unsigned argsN = count<Args>::value;

      typedef make_bin<impl_, pack<params...>, pack<param_h, arg_h, args...>> Next;
      typedef make_bin<impl_, pack<arg_h, param_h, params...>, pack<args...>> Base;

      typedef typename Next::Tail Tail;
      typedef make_bin<impl_, Pack, pack<>> Head;

      make_bin(ref<make_bin> at, Type const& base_type, arg_tuple<typename Base::Args> base_args, ref<arg_h> arg)
        : fun<param_h, pack<params...>>(at, base_type, arg->type())
        , _args(std::tuple_cat(base_args, std::make_tuple(arg)))
      { }

      ref<Val> operator()(ref<Val> arg) override {
        ref<param_h> ok = coerse<param_h>(this->h.app(), arg, this->type().from());
        new Next(this->h, this->type(), _args, ok);
        return this->h;
      }

      ref<Val> copy() const override {
        return ref<make_bin>(this->h.app(), copy_arg_tuple(arg_unpack<argsN>(), _args));
      }

    protected:
      VisitTable visit_table() const override {
        return make_visit_table<decltype(this)>::function();
      }

      arg_tuple<Args> _args;
    };

    // Tail
    template <typename impl_, typename ret, typename arg_h, typename ...args>
    struct make_bin<impl_, pack<ret>, pack<arg_h, args...>> : ret {
      typedef make_bin Self;

      typedef typename join<typename reverse<pack<args...>>::type, pack<arg_h, ret>>::type Pack;
      typedef pack<ret> Params;
      typedef typename reverse<pack<arg_h, args...>>::type Args;

      constexpr static unsigned paramsN = count<Params>::value;
      constexpr static unsigned argsN = count<Args>::value;

      typedef void Next;
      typedef make_bin<impl_, pack<arg_h, ret>, pack<args...>> Base;

      typedef Self Tail;
      typedef make_bin<impl_, Pack, pack<>> Head;

      make_bin(ref<make_bin> at, Type const& base_type, arg_tuple<typename Base::Args> base_args, ref<arg_h> arg)
        : ret(at, base_type, arg->type())
        , _args(std::tuple_cat(base_args, std::make_tuple(arg)))
      { }

      ref<Val> copy() const override {
        return ref<make_bin>(this->h.app(), copy_arg_tuple(arg_unpack<argsN>(), _args));
      }

    protected:
      VisitTable visit_table() const override {
        return make_visit_table<decltype(this)>::function();
      }

      arg_tuple<Args> _args;
    };

    // Tail, unk-returning function
    ;

    // Head, non-function values
    template <typename impl_, typename single>
    struct make_bin<impl_, pack<single>, pack<>> : single {
      typedef make_bin Self;

      typedef pack<single> Pack;
      typedef pack<single> Params;
      typedef pack<> Args;

      constexpr static unsigned paramsN = count<Params>::value;
      constexpr static unsigned argsN = count<Args>::value;

      typedef void Next;
      typedef void Base;

      typedef Self Tail;
      typedef Self Head;

      make_bin(ref<make_bin> at, arg_tuple<Args> t)
        : single(at, impl_::name)
        , _args(t)
      { }

      ref<Val> copy() const override {
        return ref<make_bin>(this->h.app(), copy_arg_tuple(arg_unpack<argsN>(), _args));
      }

    protected:
      VisitTable visit_table() const override {
        return make_visit_table<decltype(this)>::function();
      }

      arg_tuple<Args> _args;
    };

  } // namespace sel::bins_helpers

  namespace bins {

    using ll::pack;
    using bins_helpers::make_bin;
    using bins_helpers::unk;
    using bins_helpers::num;
    using bins_helpers::str;
    using bins_helpers::istr;
    using bins_helpers::lst;
    using bins_helpers::ilst;
    using bins_helpers::tpl;
    using bins_helpers::fun;

    // tonum :: Str -> Num
    // add :: Num -> Num -> Num
    // map :: (a -> b) -> [a] -> [b]
    // id :: a -> a
    // pi :: Num

    // struct crap_ : make_bin<crap_, pack<unk<'a'>, unk<'b'>, unk<'c'>, unk<'d'>>, pack<>>::Tail {
    //   void bidoof() {
    //     unk<'a'>& a = *std::get<0>(_args);
    //     auto& b = std::get<1>(_args);
    //     auto& c = std::get<2>(_args);
    //   }
    // };

    struct add_ : make_bin<add_, pack<num, num, num>, pack<>>::Tail {
      constexpr static char const* name = "add_";
      constexpr static char const* doc = "__docstr";
      using Tail::Tail;
      double value() override { return 0; }
    };

    struct tonum_ : make_bin<tonum_, pack<str, num>, pack<>>::Tail {
      constexpr static char const* name = "tonum_";
      constexpr static char const* doc = "__docstr";
      using Tail::Tail;
      double value() override { return 0; }
    };

    struct pi_ : make_bin<pi_, pack<num>, pack<>>::Tail {
      constexpr static char const* name = "pi_";
      constexpr static char const* doc = "__docstr";
      using Tail::Tail;
      double value() override { return 0; }
    };

  } // namespace sel::bins

} // namespace sel

using namespace std;
using namespace sel;

int main() {
  App app;

  // sel::ref<bins::pi_::Head> pi0(app);
  // auto pii = pi0->copy();

  // sel::ref<bins::add_::Head> add2(app);
  // sel::ref<bins::add_::Head::Next> add1 = (*add2)(pi0);
  // sel::ref<bins::add_::Head::Next::Next> add0 = (*add1)(pii);
  // add0->value();

  cout << "coucou\n";
  return 0;
}
