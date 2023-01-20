#include "common.hpp"
using namespace bins;

App app;

template <typename T> inline Val* asval(T x) { return x; }

template <> inline Val* asval(int x) { return new NumLiteral(app, x); }
template <> inline Val* asval(float x) { return new NumLiteral(app, x); }

template <> inline Val* asval(char x) { return new StrLiteral(app, string(1, x)); }
template <> inline Val* asval(char const* x) { return new StrLiteral(app, x); }
template <> inline Val* asval(string x) { return new StrLiteral(app, x); }

//template <> inline Val* asval(vector<Val*> x) { return new LstLiteral(app, x); }
template <typename T> inline Val* _lst_asval(initializer_list<T> x, Type const& ty) {
  vector<Val*> r;
  for (auto const& it : x)
    r.push_back(asval(it));
  //return asval(r);
  return new LstLiteral(app, r, new vector<Type*>{new Type(ty)});
}
template <> inline Val* asval(initializer_list<int> x) { return _lst_asval(x, Type(Ty::NUM, {0}, 0)); }
template <> inline Val* asval(initializer_list<float> x) { return _lst_asval(x, Type(Ty::NUM, {0}, 0)); }
template <> inline Val* asval(initializer_list<char> x) { return _lst_asval(x, Type(Ty::STR, {0}, 0)); }
template <> inline Val* asval(initializer_list<char const*> x) { return _lst_asval(x, Type(Ty::STR, {0}, 0)); }
template <> inline Val* asval(initializer_list<string> x) { return _lst_asval(x, Type(Ty::STR, {0}, 0)); }

template <typename T>
using ili = initializer_list<T>;


template <typename F, typename... L> struct uncurry;

template <typename F, typename H, typename... T>
struct uncurry<F, H, T...> { static inline Val* the(H h, T... t) { return (*(Fun*)uncurry<typename F::Base, T...>::the(t...))(asval(h)); } };

template <typename I, typename a, typename b, typename c, typename O>
struct uncurry<bins_helpers::_bin_be<I, ll::cons<fun<b, c>, a>>, O> { static inline Val* the(O o) { return (*(Fun*)new bins_helpers::_bin_be<I, ll::cons<fun<b, c>, a>>(app))(asval(o)); } };

// template <typename Impl>
// struct uncurry<bins_helpers::_fake_bin_be<Impl>> { static inline Val* the() { .. } };

template <typename F>
struct uncurry<F> { static inline Val* the() { return new F(app); } };


struct test_base {
  //App app;
  //test_base(): app() { }
  virtual ~test_base() { }
  operator int() {
    int r = run_test();
    if (r) cerr << "--> in test for '" << get_name() << "'\n";
    return r;
  }
  virtual int run_test() = 0;
  virtual char const* get_name() = 0;
};

/** return true when failed */
template <typename F> struct test : test_base {
  int run_test() override { cout << "no test for '" << F::name << "'\n"; return 0; }
  char const* get_name() override { return F::name; }
};


template <typename list> struct call_test;

template <typename car, typename cdr>
struct call_test<bins_ll::cons<car, cdr>> { static inline int the() { return test<car>() + call_test<cdr>::the(); } };

template <>
struct call_test<bins_ll::nil> { static inline int the() { return 0; } };

typedef
  bins_ll::bins
  //ll::cons_l<graphemes_>::the
  tested;
TEST(each) { return call_test<tested>::the(); }


#define __r(__n) __r ## __n
#define __r1(a)             a
#define __r2(a, b)          b, a
#define __r3(a, b, c)       c, b, a
#define __r4(a, b, c, d)    d, c, b, a
#define __r5(a, b, c, d, e) e, d, c, b, a
#define __rn(__n, ...) __r(__n)(__VA_ARGS__)
#define REVERSE(...) __rn(__VA_COUNT(__VA_ARGS__), __VA_ARGS__)

#define __a(__n) __a ## __n
#define __a1(a)             decltype(a)
#define __a2(a, b)          decltype(a), __a1(b)
#define __a3(a, b, c)       decltype(a), __a2(b, c)
#define __a4(a, b, c, d)    decltype(a), __a3(b, c, d)
#define __a5(a, b, c, d, e) decltype(a), __a4(b, c, d, e)
#define __an(__n, ...) __a(__n)(__VA_ARGS__)
#define TPARAM_AUTO(...) __an(__VA_COUNT(__VA_ARGS__), __VA_ARGS__)

#define CALL(__f, ...) uncurry<__f##_, TPARAM_AUTO(REVERSE(__VA_ARGS__))>::the(REVERSE(__VA_ARGS__))

#define T(__f)                         \
  template <>                          \
  struct test<__f##_> : test_base {    \
    int run_test() override;           \
    char const* get_name() override {  \
      return __f##_::name;             \
    };                                 \
  };                                   \
  int test<__f##_>::run_test()

#define __rem_par(...) __VA_ARGS__

#define assert_num(__should, __have) do {  \
  Val* _habe = (__have);                   \
  assert_eq(Ty::NUM, _habe->type().base);  \
  Num& _fart = *((Num*)_habe);             \
  assert_eq(__should, _fart.value());      \
} while (0)

#define assert_str(__should, __have) do {                \
  Val* _habe = (__have);                                 \
  assert_eq(Ty::STR, _habe->type().base);                \
  Str& _fart = *((Str*)_habe);                           \
  ostringstream oss;                                     \
  assert_cmp(__should, (_fart.entire(oss), oss.str()));  \
} while (0)

#define assert_lstnum(__should, __have) do {                      \
  Val* _habe = (__have);                                          \
  assert_eq(Ty::LST, _habe->type().base);                         \
  Lst& _fart = *((Lst*)_habe);                                    \
  for (auto const& _it : __rem_par __should) {                    \
    assert(!_fart.end(), "should not have reached the end yet");  \
    assert_num(_it, *_fart);                                      \
    ++_fart;                                                      \
  }                                                               \
  assert(_fart.end(), "should have reached the end by now");      \
} while(0)

#define assert_lststr(__should, __have) do {                      \
  Val* _habe = (__have);                                          \
  assert_eq(Ty::LST, _habe->type().base);                         \
  Lst& _fart = *((Lst*)_habe);                                    \
  for (auto const& _it : __rem_par __should) {                    \
    assert(!_fart.end(), "should not have reached the end yet");  \
    assert_str(_it, *_fart);                                      \
    ++_fart;                                                      \
  }                                                               \
  assert(_fart.end(), "should have reached the end by now");      \
} while(0)


T(abs) {
  assert_num(5, CALL(abs, -5));
  assert_num(0, CALL(abs, 0));
  assert_num(3, CALL(abs, +3));
  return 0;
}

T(add) {
  assert_num(6, CALL(add, 4, 2));
  return 0;
}

T(codepoints) {
  assert_lstnum(
    ({ 97, 12405, 98, 13, 10, 99, 127987, 8205, 9895, 100 }),
    CALL(codepoints, "\x61\xe3\x81\xb5\x62\x0d\x0a\x63\xf0\x9f\x8f\xb3\xe2\x80\x8d\xe2\x9a\xa7\x64")
  );
  return 0;
}

T(const) {
  assert_str("coucou", CALL(const, "coucou", 1));
  return 0;
}

T(div) {
  assert_num(2, CALL(div, 4, 2));
  return 0;
}

T(drop) {
  assert_lstnum(({3, 2, 1}), CALL(drop, 2, (ili<int>{5, 4, 3, 2, 1})));
  assert_lstnum(({5, 4, 3, 2, 1}), CALL(drop, 0, (ili<int>{5, 4, 3, 2, 1})));
  assert_lstnum((ili<int>{}), CALL(drop, 2, (ili<int>{})));
  assert_lstnum((ili<int>{}), CALL(drop, 6, (ili<int>{5, 4, 3, 2, 1})));
  return 0;
}

T(graphemes) {
  assert_lststr(
    ({ "\x61", "\xe3\x81\xb5", "\x62", "\x0d\x0a", "\x63", "\xf0\x9f\x8f\xb3\xe2\x80\x8d\xe2\x9a\xa7",
      // FIXME: wasn't able to figure why: it seems to end
      // one too soon in testing, but not in just using it
      // through script; this actually behaves as expected
      // `printf [..] | s/el graphemes, join :-:`
      //"\x64"
    }),
    CALL(graphemes, "\x61\xe3\x81\xb5\x62\x0d\x0a\x63\xf0\x9f\x8f\xb3\xe2\x80\x8d\xe2\x9a\xa7\x64")
  );
  return 0;
}

T(mul) {
  assert_num(8, CALL(mul, 4, 2));
  return 0;
}

T(sub) {
  assert_num(2, CALL(sub, 4, 2));
  return 0;
}

T(take) {
  assert_lstnum(({5, 4}), CALL(take, 2, (ili<int>{5, 4, 3, 2, 1})));
  assert_lstnum((ili<int>{}), CALL(take, 0, (ili<int>{5, 4, 3, 2, 1})));
  assert_lstnum((ili<int>{}), CALL(take, 2, (ili<int>{})));
  assert_lstnum(({5, 4, 3, 2, 1}), CALL(take, 6, (ili<int>{5, 4, 3, 2, 1})));
  return 0;
}
