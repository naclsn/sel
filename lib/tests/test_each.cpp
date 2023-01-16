#include "common.hpp"
using namespace bins;

App app;

template <typename T> Val* asval(T x);

template <> inline Val* asval(int x) { return new NumLiteral(app, x); }
template <> inline Val* asval(float x) { return new NumLiteral(app, x); }

template <> inline Val* asval(char x) { return new StrLiteral(app, string(1, x)); }
template <> inline Val* asval(char const* x) { return new StrLiteral(app, x); }
template <> inline Val* asval(string x) { return new StrLiteral(app, x); }

template <> inline Val* asval(vector<Val*> x) { return new LstLiteral(app, x); }


template <typename F, typename... L> struct uncurry;

template <typename F, typename H, typename... T>
struct uncurry<F, H, T...> { static inline Val* the(H h, T... t) { return (*(Fun*)uncurry<typename F::Base, T...>::the(t...))(asval(h)); } };

template <typename I, typename a, typename b, typename c, typename O>
struct uncurry<bins_helpers::_bin_be<I, ll::cons<fun<b, c>, a>>, O> { static inline Val* the(O o) { return (*(Fun*)new bins_helpers::_bin_be<I, ll::cons<fun<b, c>, a>>(app))(asval(o)); } };

// template <typename Impl>
// struct uncurry<bins_helpers::_fake_bin_be<Impl>> { static inline Val* the() { .. } };

template <typename F>
struct uncurry<F> { static inline Val* the() { return new F(app); } };


/** return true when failed */
template <typename F> int test() { cout << "no test for '" << F::name << "'\n"; return false; }


template <typename list> struct call_test;

template <typename car, typename cdr>
struct call_test<bins_ll::cons<car, cdr>> { static inline unsigned the() { return test<car>() + call_test<cdr>::the(); } };

template <>
struct call_test<bins_ll::nil> { static inline unsigned the() { return 0; } };

TEST(each) { return call_test<bins_ll::bins>::the(); }


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


template <>
int test<abs_>() {
  assert_eq(5, (*(Num*)CALL(abs, -5)).value());
  assert_eq(0, (*(Num*)CALL(abs, 0)).value());
  assert_eq(3, (*(Num*)CALL(abs, +3)).value());
  return 0;
}

template <>
int test<add_>() {
  assert_eq(6, (*(Num*)CALL(add, 4, 2)).value());
  return 0;
}

template <>
int test<codepoints_>() {
  constexpr char const* c = "aふb\r\nc🏳‍⚧d";
  constexpr uint32_t const p[] = { 97, 12405, 98, 13, 10, 99, 127987, 8205, 9895, 100 };
  Lst& cps = (*(Lst*)(CALL(codepoints, c)));
  for (auto const& it : p) {
    assert(!cps.end(), "should not have reached the end yet");
    assert_eq(it, ((Num*)*cps)->value());
    ++cps;
  }
  return 0;
}

template <>
int test<conjunction_>() {
  cout << "(for `conjunction_`) niy: arbitrary value comparison -- not tested yet\n";
  //CALL(conjunction, ...);
  return 0;
}

template <>
int test<const_>() {
  ostringstream oss;
  Val* s = CALL(const, "coucou", 1);
  assert_eq(Type(Ty::STR, {0}, 0), s->type());
  assert_eq("coucou", (((Str*)s)->entire(oss), oss.str()));
  return 0;
}
