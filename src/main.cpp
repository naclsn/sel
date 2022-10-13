#include <iostream>

#include "value.hpp"

int main() {
  Engine::Val* a = new Engine::Num(1);
  Engine::Val* b = new Engine::Num(2);

  Engine::Fun* add;

  std::cout << *a->asNum() + *b->asNum() << std::endl;

  delete a;
  delete b;

  return 0;
}
