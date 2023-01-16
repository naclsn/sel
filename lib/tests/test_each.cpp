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
template <typename F> bool test() { cout << "no test for '" << F::name << "'\n"; return false; }


template <typename list> struct call_test;

template <typename car, typename cdr>
struct call_test<bins_ll::cons<car, cdr>> { static inline unsigned the() { return test<car>() + call_test<cdr>::the(); } };

template <>
struct call_test<bins_ll::nil> { static inline unsigned the() { return 0; } };

TEST(each) {
  return call_test<bins_ll::bins>::the();
}


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

#define CALL(__f, ...) uncurry<__f, TPARAM_AUTO(REVERSE(__VA_ARGS__))>::the(REVERSE(__VA_ARGS__))


template <>
bool test<abs_>() {
  Val* r = CALL(abs_, -1);
  cout << "r: " << repr(*r) << '\n';
  cout << (*(Num*)r).value() << endl;
  return false;
}

template <>
bool test<const_>()  {
  Val* r = CALL(const_, "coucou", 1);
  cout << "r: " << repr(*r) << '\n';
  return false;
}
