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
#define RESET "\e[m"

#define fail(__msg) {                    \
  cerr                                   \
    << "failed with:\n"                  \
    << "   " RED << __msg << RESET "\n"  \
  ;                                      \
  exit(EXIT_FAILURE);                    \
}

#define assert(__expr, __msg) {          \
  if (__expr) return;                    \
  cerr                                   \
    << RED #__expr "\n"                  \
    << "assertion failed with:\n"        \
    << "   " RED << __msg << RESET "\n"  \
  ;                                      \
  exit(EXIT_FAILURE);                    \
}

#define assert_eq(__should, __have) {                         \
  if (__should == __have) return;                             \
  cerr                                                        \
    << GREEN #__should RESET "  ==  " RED #__have RESET "\n"  \
    << "assertion failed with:\n"                             \
    << "   expected: " GREEN << __should << RESET "\n"        \
    << "    but got: " RED   << __have   << RESET "\n"        \
  ;                                                           \
  exit(EXIT_FAILURE);                                         \
}

#define assert_cmp assert_eq
// #define assert_cmp(__should, __have) _assert_cmp(#__should, #__have, __should, __have)
// inline void _assert_cmp(char const* sshould, char const* shave, string should, string have) {
// }

#define TEST(__function)  \
  void __##__function();  \
  int main() {            \
    __##__function();     \
    return EXIT_SUCCESS;  \
  }                       \
  void __##__function()
