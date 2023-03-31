#include "sel/engine.hpp"
#include "sel/errors.hpp"
#include "sel/utils.hpp"

namespace sel {

    std::string TypeError::bidoof(Val const& from, Type const& to) {
      std::ostringstream oss;
      if (Ty::FUN == to.base()) {
        if (Ty::FUN == from.type().base())
          oss << "function arity of " << utils::show(from) << " :: " << from.type() << " does not match with " << to;
        else oss << "value of type " << utils::show(from) << " :: " << from.type() << " is not a function";
      } else oss << "cannot coerse from " << utils::show(from) << " :: " << from.type() << " to " << to;
      return oss.str();
    }

} // namespace sel
