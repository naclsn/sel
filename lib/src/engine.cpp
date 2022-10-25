#include "sel/errors.hpp"
#include "sel/engine.hpp"

namespace sel {

  void Val::accept(Visitor& v) const { throw NIYError("visitor pattern for this class (idk which)", "- what -"); }

  template <typename To>
  To* coerse(Val* from) { return (To*)from; }

  template Num* coerse(Val*);
  template Str* coerse(Val*);
  template Lst* coerse(Val*);
  template Fun* coerse(Val*);

} // namespace sel
