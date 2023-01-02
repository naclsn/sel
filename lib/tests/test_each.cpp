#include "common.hpp"
using namespace bins;

template <typename T> Val* asval(T x);

template <> Val* asval(int x) { return new NumLiteral(x); }
template <> Val* asval(float x) { return new NumLiteral(x); }

template <> Val* asval(char const* x) { return new StrLiteral(x); }
template <> Val* asval(string x) { return new StrLiteral(x); }


template <typename F, typename ...L>
struct uncurry;

template <typename F, typename H, typename ...T>
struct uncurry<F, H, T...> { static inline Val* the(H h, T... t) { return (*(Fun*)uncurry<typename F::Base>::the(t...))(asval(h)); } };

template <typename F>
struct uncurry<F> { static inline Val* the() { return new F(); } };


template <typename F> bool test() { cerr << "no test for '" << F::name << "'\n"; return true; }


template <typename list> struct call_test;

template <typename car, typename cdr>
struct call_test<bins_ll::cons<car, cdr>> { static inline bool the() { return test<car>() && call_test<cdr>::the(); } };

template <>
struct call_test<bins_ll::nil> { static inline bool the() { return true; } };


template <>
bool test<abs_>() {
  Val* r = uncurry<abs_, int>::the(1);
  cout << "r: " << repr(*r) << '\n';
  return true;
}

TEST(each) {
  call_test<bins_ll::bins>::the();
}
