#ifndef SEL_PARSER_HPP
#define SEL_PARSER_HPP

/**
 * Parsing of the main language and construction of an
 * Application from a user script.
 */

#include <ostream>
#include <istream>
#include <string>

#include "engine.hpp"

namespace sel {

  class NumLiteral : public Num {
    double const n;
  public:
    NumLiteral(double n): n(n) { }
    double value() override { return n; }
    void accept(Visitor& v) const override { v.visitNumLiteral(type(), n); }
  };

  std::ostream& operator<<(std::ostream& out, Application const& ty);
  std::istream& operator>>(std::istream& in, Application& tt);

} // namespace sel

#endif // SEL_PARSER_HPP
