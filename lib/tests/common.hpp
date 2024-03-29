#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include "sel/application.hpp"
#include "sel/builtins.hpp"
#include "sel/parser.hpp"
#include "sel/types.hpp"
#include "sel/utils.hpp"

using namespace std;
using namespace sel;

typedef vector<Type> Types;
typedef vector<unique_ptr<Val>> Vals;

template <typename H, typename ...T>
struct push_many {
  static inline void function(Vals& v, H h, T... t) {
    v.push_back(forward<H>(h));
    push_many<T...>::function(v, forward<T>(t)...);
  }
};
template <typename O>
struct push_many<O> {
  static inline void function(Vals& v, O o) {
    v.push_back(forward<O>(o));
  }
};

template <typename ...Args>
Vals make_vals(Args... args) {
  Vals r;
  push_many<Args...>::function(r, forward<Args>(args)...);
  return r;
}
template <>
Vals make_vals() {
  return Vals();
}

#define RED   "\e[31m"
#define GREEN "\e[32m"
#define BLUE  "\e[34m"
#define RESET "\e[m"

#define report(__for, __nbf) {                                        \
  int _nbf = (__nbf);                                                 \
  cerr << __for << ": " << _nbf << " fail" << (_nbf < 2 ? "" : "s");  \
} while (0)

#define fail(__msg) do {                 \
  cerr                                   \
    << "failed with:\n"                  \
    << "   " RED << __msg << RESET "\n"  \
  ;                                      \
  return 1;                              \
} while (0)

#define assert(__expr, __msg) do {       \
  if (__expr) break;                     \
  cerr                                   \
    << RED #__expr "\n"                  \
    << "assertion failed with:\n"        \
    << "   " RED << __msg << RESET "\n"  \
  ;                                      \
  return 1;                              \
} while (0)

#define assert_eq(__should, __have) do {                      \
  auto _should = (__should);                                  \
  auto _have = (__have);                                      \
  if (_should == _have) break;                                \
  cerr                                                        \
    << GREEN #__should RESET "  ==  " RED #__have RESET "\n"  \
    << "assertion failed with:\n"                             \
    << "   values do not match\n"                             \
    << "   expected: " GREEN << _should << RESET "\n"         \
    << "    but got: " RED   << _have   << RESET "\n"         \
  ;                                                           \
  return 1;                                                   \
} while (0)

#define assert_ne(__shldnt, __have) do {                      \
  auto _shldnt = (__shldnt);                                  \
  auto _have = (__have);                                      \
  if (_shldnt != _have) break;                                \
  cerr                                                        \
    << GREEN #__shldnt RESET "  !=  " RED #__have RESET "\n"  \
    << "assertion failed with:\n"                             \
    << "   values do match\n"                                 \
    << "   unexpected: " GREEN << _shldnt << RESET "\n"       \
    << "   but indeed: " RED   << _have   << RESET "\n"       \
  ;                                                           \
  return 1;                                                   \
} while (0)

#define assert_cmp(__should, __have) do {                 \
  if (_assert_cmp(#__should, #__have, __should, __have))  \
    return 1;                                             \
} while (0)

inline bool _assert_cmp(char const* sshould, char const* shave, string should, string have) {
  if (should == have) return false;

  cerr
    << GREEN << sshould << RESET "  ==  " RED << shave << RESET "\n"
    << "assertion failed with:\n"
    << "   texts do not match\n"
  ;

  string::size_type a_st = 0, a_ed;
  string::size_type b_st = 0, b_ed;
  while (true) {
    a_ed = should.find('\n', a_st);
    b_ed = have.find('\n', b_st);

    string la = should.substr(a_st, a_ed-a_st);
    string lb = have.substr(b_st, b_ed-b_st);

    if (la == lb) cerr << la << "\n";
    else cerr
      << GREEN << la << RESET "\n"
      << RED   << lb << RESET "\n"
    ;

    if (string::npos == a_ed || string::npos == b_ed) break;
    a_st = a_ed+1;
    b_st = b_ed+1;
  }

  cerr << "\n";

  return true;
}

#define TEST(__function)           \
  int __##__function();            \
  int main() {                     \
    int fails = __##__function();  \
    report(#__function, fails);    \
    return 0 == fails              \
      ? EXIT_SUCCESS               \
      : EXIT_FAILURE;              \
  }                                \
  int __##__function()
