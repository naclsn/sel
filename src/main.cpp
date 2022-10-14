#include <iostream>

#include "error.hpp"
#include "value.hpp"
#include "number.hpp"
#include "string.hpp"
#include "list.hpp"
#include "function.hpp"

using namespace std;
using namespace sel;

class FunAdd : public Fun, public NumFloat {
protected:
  void eval() override {
    TRACE(FunAdd::eval);
    setValue(a->value() + b->value());
  }
  std::ostream& output(std::ostream& out) override {
    return NumFloat::output(out);
  }
public:
  Num* a;
  Num* b;
  FunAdd(): Fun(
    new Type(Ty::NUM),
    new Type(Ty::FUN,
      new Type(Ty::NUM),
      new Type(Ty::NUM)
    )
  ), /*NumFloat(),*/ a(nullptr), b(nullptr) { }
  ~FunAdd() {
    TRACE(~FunAdd);
    delete a;
    delete b;
  }
  Val* apply(Val* arg) override {
    if (!a) { a = (Num*)arg->coerse(Ty::NUM); return (Fun*)this; }
    if (!b) { b = (Num*)arg->coerse(Ty::NUM); return (Fun*)this; }
    throw ParameterError(arg, "FunAdd::apply");
  }
};

void some() {
  Val* a = new NumFloat(1);
  Val* b = new NumFloat(1);

  cout << "a :: " << a->type() << " = " << *a << endl;
  cout << "b :: " << b->type() << " = " << *b << endl;

  Val* add0 = (Fun*)new FunAdd();
  Val* add1 = ((Fun*)add0)->apply((Num*)a);
  Val* res = ((Fun*)add1)->apply((Num*)b);

  cout << "res :: " << res->type() << " = " << *res << endl;

  delete res;
}

int main() {
  try {
    some();
    return EXIT_SUCCESS;

  } catch (TypeError& e) {
    cerr
      << "got error:" << endl
      << e.what() << endl
    ;
    return EXIT_FAILURE;
  }
}
