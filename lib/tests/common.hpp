#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "sel/types.hpp"

using namespace std;
using namespace sel;

#define fail(__msg) {         \
  cerr                        \
    << "failed with:\n"       \
    << "  " << __msg << endl  \
  ;                           \
  exit(EXIT_FAILURE);         \
}

#define assert(__expr, __msg) {    \
  if (__expr) return;              \
  cerr                             \
    << "assertion failed with:\n"  \
    << "  " << __msg << endl       \
  ;                                \
  exit(EXIT_FAILURE);              \
}

#define assert_eq(__should, __have) {      \
  if (__should == __have) return;          \
  cerr                                     \
    << "assertion failed with:\n"          \
    << "  expected: " << __should << endl  \
    << "   but got: " << __have << endl    \
  ;                                        \
  exit(EXIT_FAILURE);                      \
}

#define TEST(__function)  \
  void __function();      \
  int main() {            \
    __function();         \
    return EXIT_SUCCESS;  \
  }                       \
  void __function()
