#include "sel/unicode.hpp"

namespace sel {

  std::ostream& operator<<(std::ostream& out, codepoint const& r) {
    uint32_t a = r.u;
    if (a < 0b10000000) out.put(a);
    else {
      char x = a & 0b00111111;
      a>>= 6;
      if (a < 0b00100000) out.put(0b11000000 | a);
      else {
        char y = a & 0b00111111;
        a>>= 6;
        if (a < 0b00010000) out.put(0b11100000 | a);
        else {
          char z = a & 0b00111111;
          out.put(0b11110000 | (a >> 6));
          out.put(0b10000000 | z);
        }
        out.put(0b10000000 | y);
      }
      out.put(0b10000000 | x);
    }
    return out;
  }

  std::istream& operator>>(std::istream& in, codepoint& r) {
    char a = in.get();
    if (0 == (0b10000000 & a)) r.u = a;
    else if (0 == (0b00100000 & a)) r.u = ((a & 0b00011111) << 6) | (in.get() & 0b00111111);
    else if (0 == (0b00010000 & a)) r.u = ((a & 0b00001111) << 12) | ((in.get() & 0b00111111) << 6) | (in.get() & 0b00111111);
    else if (0 == (0b00001000 & a)) r.u = ((a & 0b00000111) << 18) | ((in.get() & 0b00111111) << 12)  | ((in.get() & 0b00111111) << 6) | (in.get() & 0b00111111);
    return in;
  }

  void grapheme::push_back(codepoint const& cp) {
    if (0 == many.zero) return many.vec.push_back(cp);
    int c = 1;
    while (few[c].u)
      if (inl == ++c) {
        std::vector<codepoint> onstack;
        std::copy(few, few+inl, onstack.begin());
        many.zero = 0;
        many.vec = onstack;
        return;
      }
    few[c] = cp;
  }

  grapheme::size_type grapheme::size() const {
    if (0 == many.zero) return many.vec.size();
    int c = 1;
    while (few[c].u)
      if (inl == ++c) return inl;
    return c;
  }

  codepoint grapheme::at(grapheme::size_type n) const {
    if (0 == many.zero) return many.vec.at(n);
    return few[n];
  }

  void read_grapheme(std::istream_iterator<codepoint>& it, grapheme& r) {
    static std::istream_iterator<codepoint> eos;
    ; // TODO: niy
  }

  std::ostream& operator<<(std::ostream& out, grapheme const& r) {
    grapheme::size_type l = r.size();
    for (grapheme::size_type k = 0; k < l; k++)
      out << r.at(k);
    return out;
  }

  std::istream& operator>>(std::istream& in, grapheme& r) {
    std::istream_iterator<codepoint> it(in);
    read_grapheme(it, r);
    return in;
  }


} // namespace sel
