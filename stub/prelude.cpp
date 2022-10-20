// place holder file
#include "sel/engine.hpp"
#include "sel/visitors.hpp"
#include "prelude.hpp"

namespace sel {

  Val* Add2::operator()(Val* arg) {
    return new Add1(this, coerse<Num>(arg));
  }
  void Add2::accept(Visitor& v) const {
    v.visitAdd2(type());
  }

  Val* Add1::operator()(Val* arg) {
    return new Add0(this, coerse<Num>(arg));
  }
  void Add1::accept(Visitor& v) const {
    v.visitAdd1(type(), base, arg);
  }

  double Add0::value() {
    return base->arg->value() + arg->value();
  }
  void Add0::accept(Visitor& v) const {
    v.visitAdd0(type(), base, arg);
  }

} // namespace sel
