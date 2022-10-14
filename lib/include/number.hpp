#ifndef SEL_NUMBER_HPP
#define SEL_NUMBER_HPP

#include "value.hpp"

namespace sel {

  class Num : public Val {
  private:
    float n;

  protected:
    void eval() override { }
    std::ostream& output(std::ostream& out) override {
      return out << n;
    }

  public:
    Num(float f): Val(Ty::NUM), n(f) { }
    VERB_DTOR(Num, { });

    Val* coerse(Type to) override;
  }; // class Num

}

#endif // SEL_NUMBER_HPP
