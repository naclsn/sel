#include <iostream> // debugging(cerr): remove when ok

#include "sel/parser.hpp"

namespace sel {

  std::ostream& operator<<(std::ostream& out, Application const& ty) {
    std::cerr << "TODO: operator<< for Application\n";
    return out;
  }

  std::istream& operator>>(std::istream& in, Application& tt) {
    std::cerr << "TODO: operator>> for Application\n";
    return in;
  }

} // namespace sel
