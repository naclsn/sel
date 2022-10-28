#ifndef SEL_BUILTINS_HPP
#define SEL_BUILTINS_HPP

#include <string>

#include "sel/utils.hpp"
#include "sel/engine.hpp"

namespace sel {

  /**
   * Seach for a value by name, return nullptr if not found.
   */
  Val* lookup_name(std::string const& name);

} // namespace sel

#endif // SEL_BUILTINS_HPP
