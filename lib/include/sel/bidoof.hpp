#ifndef SEL_BIDOOF_HPP
#define SEL_BIDOOF_HPP

#include "types.hpp"
#include "engine.hpp"

namespace sel {

  // ZZZ: test class
  class Bidoof : public Val {
  private:
    char const* some;
    Bidoof const* other;
  public:
    Bidoof()
      : Val(numType())
      , some("bidoof-some")
      , other(new Bidoof("bidoof-deep"))
    { }
    Bidoof(char const* some)
      : Val(strType(TyFlag::IS_INF))
      , some(some)
      , other(nullptr)
    { }
    ~Bidoof() { delete other; }
    void accept(Visitor& v) const override { v.visitBidoof(type(), some, (Val*)other); }
  };

}

#endif // SEL_BIDOOF_HPP
