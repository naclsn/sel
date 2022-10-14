#ifndef SEL_NUMBER_HPP
#define SEL_NUMBER_HPP

#include "value.hpp"

namespace sel {

  class Num : public Val {
  public:
    Num(): Val(Ty::NUM) { }
    ~Num() { }

    Val* coerse(Type to) override;
  }; // class Num

  class NumFloat : public Num {
  private:
    float n;

  protected:
    void eval() override { }
    std::ostream& output(std::ostream& out) override { return out << n; }

  public:
    NumFloat(float f): n(f) { }
    ~NumFloat() { }

    Val* clone() override;
    Val* coerse(Type to) override;
  }; // class Num

}

#endif // SEL_NUMBER_HPP
