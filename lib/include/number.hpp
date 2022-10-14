#ifndef SEL_NUMBER_HPP
#define SEL_NUMBER_HPP

#include "value.hpp"

namespace sel {

  class Num : public Val {
  public:
    Num(): Val(new Type(Ty::NUM)) { }
    ~Num() {
      TRACE(~Num);
    }

    Val* coerse(Type to) override;

    virtual float value() = 0;
  }; // class Num

  class NumFloat : public Num {
  private:
    float n;

  protected:
    void eval() override;
    std::ostream& output(std::ostream& out) override;

    void setValue(float f) { n = f; }

  public:
    NumFloat() { }
    NumFloat(float f): n(f) { }
    ~NumFloat() {
      TRACE(~NumFloat);
    }

    // Val* clone() const override;
    float value() override {
      eval();
      return n;
    }
  }; // class Num

}

#endif // SEL_NUMBER_HPP
