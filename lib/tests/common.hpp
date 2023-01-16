#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "sel/types.hpp"
#include "sel/parser.hpp"

using namespace std;
using namespace sel;

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
    << "   expected: " GREEN << _should << RESET "\n"         \
    << "    but got: " RED   << _have   << RESET "\n"         \
  ;                                                           \
  return 1;                                                   \
} while (0)

#define assert_cmp assert_eq
// #define assert_cmp(__should, __have) _assert_cmp(#__should, #__have, __should, __have)
// inline void _assert_cmp(char const* sshould, char const* shave, string should, string have) {
// }

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
