#ifndef SEL_UNICODE_HPP
#define SEL_UNICODE_HPP

/**
 * Simplest things you could have for UTF-8.
 * To use with istream iterator.
 */

#include <iostream>

namespace sel {

  struct codepoint {
    uint32_t u;
    template <typename T>
    codepoint(T u): u(u) { }
  };

  std::ostream& operator<<(std::ostream& out, codepoint const& r);
  std::istream& operator>>(std::istream& in, codepoint& r);

} // namespace sel

#endif // SEL_UNICODE_HPP
