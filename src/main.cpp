#include <iostream>

#include "value.hpp"

using namespace std;
using namespace sel;

int main() {
  try {
    Num const* true_a = new Num(1);

    Val const& a = *true_a;
    cout
      << "this is the original number: " << a.typed() << endl
      << "  and here is the value: " << a << endl;

    Val const& b = *a.coerse(BasicType::STR);
    cout
      << "this is supposed to now be a string: " << b.typed() << endl
      << "  and here is the value: " << b << endl;

    Val const& c = *b.coerse(BasicType::NUM);
    cout
      << "this is supposed to now be a number again: " << c.typed() << endl;
      // << "  and here is the value: " << c << endl;

    delete &c;
    return EXIT_SUCCESS;

  } catch (TypeError const& e) {
    cerr
      << "got error:" << endl
      << e.what() << endl
    ;
    return EXIT_FAILURE;
  }
}
