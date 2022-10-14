#ifndef SEL_FUNCTION_HPP
#define SEL_FUNCTION_HPP

#include "value.hpp"

namespace sel {

  class Fun : public Val {
  protected:
    // void eval() override { }
    std::ostream& output(std::ostream& out) override { return out; }

  public:
    Fun(Type* from, Type* to): Val(new Type(Ty::FUN, from, to)) { }
    ~Fun() {
      TRACE(~Fun);
    }

    Val* coerse(Type to) override;

    virtual Val* apply(Val* arg) = 0;
  };

}

#endif // SEL_FUNCTION_HPP
