#include <tuple>
#include "sel/ll.hpp"
using namespace sel::ll;

struct Type;
struct Val;
template <typename> struct ref { };
template <typename> struct arg_tuple { };

// makes a pack 'a, b, c..' into a type 'a -> b -> c..'
template <typename PackItself> struct make_fun_from_pack;
template <typename h, typename ...t>
struct make_fun_from_pack<pack<h, t...>> {
  typedef fun<h, typename make_fun_from_pack<t...>::type> type;
};
template <typename o>
struct make_fun_from_pack<pack<o>> {
  typedef o type;
};
template <typename PackItself> using ty_from_pack = typename make_fun_from_pack<PackItself>::type;

template <typename impl_, typename Params, typename rArgs>
struct make_bin : ty_from_pack<Params> {
  typedef typename reverse<rArgs>::type Args;
  typedef typename join<Args, Params>::type Pack;

  constexpr static unsigned paramsN = count<Params>::value;
  constexpr static unsigned argsN = count<Args>::value;

  typedef head_tail<Params> _pht;
  typedef head_tail<rArgs> _aht;
  typedef make_bin<impl_, typename _pht::tail, typename join<pack<typename _pht::head>, rArgs>::type> Next;
  typedef make_bin<impl_, typename join<pack<typename _aht::head>, Params>::type, typename _aht::tail> Base;

  typedef typename Next::Tail Tail;
  typedef make_bin<impl_, Pack, pack<>> Head;

  typedef typename std::conditional
    < 1 < count<Params>::value
    , make_bin // in Head/Body
    , impl_ // in Tail/Sole
    >::type _concrete;

  // constructor for both 'make_at' and 'copy'
  make_bin(ref<Val> at, Type&& ty, arg_tuple<Args> t)
    : ty_from_pack<Params>(at, std::forward<Type>(ty))
    , _args(t)
  { }

  // YYY: this only exists for Heads
  make_bin(ref<Val> at)
    : ty_from_pack<Params>(at, ty_from_pack<Params>::make(impl_::name))
    , _args({})
  { }

  ref<Val> copy() const override {
    return ref<_concrete>(this->h.app(), Type(this->ty), copy_arg_tuple(arg_unpack<argsN>(), this->_args));
  }

  // YYY: this is only used in Body(_concrete=make_bin)/Tail(_concrete=impl_)
  static inline void make_at(ref<Val> at, Type const& base_type, arg_tuple<typename Base::Args> base_args, ref<Val> arg) {
    new _concrete(at, done_applied(base_type, arg->type()), std::tuple_cat(base_args, std::make_tuple(arg)));
  }

  // YYY: only exists for Head/Body
  typename std::enable_if
    < 1 < count<Params>::value
    , ref<Val>
    >::type 
  operator()(ref<Val> arg) override {
    ref<Val> ok = coerse<typename _pht::head::base_type>(this->h.app(), arg, this->ty.from());
    auto copy = this->h;
    Next::make_at(this->h, this->ty, _args, ok);
    return copy; // this->h would be accessing into deleted object
  }

  arg_tuple<Args> const& args() { return _args; }

protected:
  VisitTable visit_table() const override {
    return make_visit_table<decltype(this)>::function();
  }

  arg_tuple<Args> _args;
};
