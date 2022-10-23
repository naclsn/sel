// place holder file
#ifndef SEL_PRELUDE_HPP
#define SEL_PRELUDE_HPP

#include "sel/utils.hpp"

#include <string>

#include "sel/engine.hpp"

namespace sel {

  // return may be nullptr
  Val* lookup_name(Env& env, std::string const& name);
  Fun* lookup_unary(Env& env, std::string const& name);
  Fun* lookup_binary(Env& env, std::string const& name);

}

#endif // SEL_PRELUDE_HPP
