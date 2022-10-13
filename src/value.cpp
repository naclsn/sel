#include "value.hpp"

namespace Engine {

  const Str* Num::toStr() const { return new Str(""); }

  const Num* Str::toNum() const { return new Num(.0); }
  // const Lst* Str::toNumLst() const { return new Lst(); }
  // const Lst* Str::toStrLst() const { return new Lst(); }

}
