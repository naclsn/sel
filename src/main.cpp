#include <iostream>

#include "error.hpp"
#include "value.hpp"
#include "number.hpp"
#include "string.hpp"

using namespace std;
using namespace sel;

int main() {
  try {
    Num* true_a = new Num(1);

    Val& a = *true_a;
    cout
      << "this is the original number: " << a.typed() << endl
      << "  and here is the value: " << a << endl;

    Val& b = *a.coerse(Ty::STR);
    cout
      << "this is supposed to now be a string: " << b.typed() << endl
      << "  and here is the value: " << b << endl;

    // Val& c = *b.coerse(Ty::NUM);
    // cout
    //   << "this is supposed to now be a number again: " << c.typed() << endl;
    //   // << "  and here is the value: " << c << endl;

    // delete &c;
    return EXIT_SUCCESS;

  } catch (TypeError& e) {
    cerr
      << "got error:" << endl
      << e.what() << endl
    ;
    return EXIT_FAILURE;
  }
}
