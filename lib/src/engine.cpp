#include <typeinfo>

#include "sel/errors.hpp"
#include "sel/engine.hpp"

namespace sel {

  void Val::accept(Visitor& v) const {
    throw NIYError(std::string("'accept' of visitor pattern for this class: ") + typeid(*this).name());
  }

  template <typename To>
  To* coerse(Val* from) { return (To*)from; }

  template Val* coerse(Val*);
  template Num* coerse(Val*);
  template Str* coerse(Val*);
  template Lst* coerse(Val*);
  template Fun* coerse(Val*);

} // namespace sel
