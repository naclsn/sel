#include "sel/engine.hpp"

namespace sel {

  template <typename To>
  To* coerse(Val* from) { return (To*)from; }

} // namespace sel
