#include "sel/engine.hpp"

namespace sel {

  template <typename To>
  To* coerse(Val* from) { return (To*)from; }

  template Num* coerse(Val*);
  template Str* coerse(Val*);
  template Lst* coerse(Val*);
  // template Cpl* coerse(Val*);

} // namespace sel
