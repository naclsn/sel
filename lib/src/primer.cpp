#include <iostream>

#include "sel/types.hpp"
#include "sel/engine.hpp"

using namespace std;

int main(int argv, char const* argc[]) {
  cout << "=== primer ===" << endl;
  for (int k = 0; k < argv; k++)
    cout << "\t" << argc[k] << "\n";
  cout << endl;
  return EXIT_SUCCESS;
}
