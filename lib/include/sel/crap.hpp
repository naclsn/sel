#ifndef CRAP
#define CRAP

#include <string>

#include "types.hpp"
#include "engine.hpp"

namespace sel {

  // ZZZ: test class
  class Crap : public Val {
  private:
    char const* some;
    Crap const* other;
  public:
    Crap()
      : Val(numType())
      , some("crap.some")
      , other(new Crap("crap.recursive"))
    { }
    Crap(char const* some)
      : Val(numType())
      , some(some)
      , other(nullptr)
    { }
    ~Crap() { delete other; }
    void accept(Visitor& v) const override { v.visitCrap(some, (Val*)other); }
  };

}

#endif // CRAP
