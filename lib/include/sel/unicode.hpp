#ifndef SEL_UNICODE_HPP
#define SEL_UNICODE_HPP

/**
 * Simplest things you could have for UTF-8.
 * To use with istream iterator.
 */

#include <iostream>
#include <iterator>
#include <vector>

namespace sel {

  struct codepoint {
    uint32_t u = 0;
    codepoint() { }
    template <typename T>
    codepoint(T u): u(u) { }
  };

  std::ostream& operator<<(std::ostream& out, codepoint const& r);
  std::istream& operator>>(std::istream& in, codepoint& r);

  union grapheme {
  private:
    struct _dyn_codepoint {
      unsigned zero = 0;
      std::vector<codepoint> vec;
    };
    constexpr static int inl = sizeof(_dyn_codepoint) / sizeof(codepoint);
    static_assert(1 < inl, "probly architecture not supported idk");

    codepoint few[inl];
    _dyn_codepoint many;

  public:
    typedef std::vector<codepoint>::size_type size_type;

    // the character \0 forms a 1-codepoint grapheme by
    // default (GB999, 'Other'); let's assume it would not
    // appear too often in bytes that actually represent
    // characters
    grapheme(codepoint const& first) {
      if (!first.u) {
        many.vec = std::vector<codepoint>({first});
      } else few[0] = first;
    }
    // this one will never be called with \0 anywhere;
    // it's here as a shortcut over new().push_back()
    // for cases such as \r\n
    grapheme(codepoint const& first, codepoint const& second) {
      few[0] = first;
      few[1] = second;
    }
    ~grapheme() { if (0 == many.zero) many.vec.~vector(); }

    void push_back(codepoint const& cp);
    size_type size() const;
    codepoint at(size_type n) const;
  }; // grapheme

  void read_grapheme(std::istream_iterator<codepoint>& it, grapheme& r);

  std::ostream& operator<<(std::ostream& out, grapheme const& r);
  std::istream& operator>>(std::istream& in, grapheme& r);

} // namespace sel

#endif // SEL_UNICODE_HPP
