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
    // XXX: add checks on each get for eof and for garbled
    // garbage (but i don't think this can fail too badly
    // and its not like we can do anything on broken
    // input either)
    char a = in.get();
    if (0 == (0b10000000 & a)) r.u = a;
    else if (0 == (0b00100000 & a)) r.u = ((a & 0b00011111) << 6) | (in.get() & 0b00111111);
    else if (0 == (0b00010000 & a)) r.u = ((a & 0b00001111) << 12) | ((in.get() & 0b00111111) << 6) | (in.get() & 0b00111111);
    else if (0 == (0b00001000 & a)) r.u = ((a & 0b00000111) << 18) | ((in.get() & 0b00111111) << 12)  | ((in.get() & 0b00111111) << 6) | (in.get() & 0b00111111);
    return in;
  }

  grapheme& grapheme::operator=(grapheme const& cp) {
    if (this == &cp) return *this;
    if (0 == cp.many.zero) {
      many.zero = 0;
      many.vec = cp.many.vec;
    } else std::copy(cp.few, cp.few+inl, few);
    return *this;
  }

  void grapheme::push_back(codepoint const& cp) {
    if (0 == many.zero) return many.vec.push_back(cp);
    int c = 1;
    while (few[c].u)
      if (inl == ++c) {
        // XXX: is this guaranteed this will live on the
        // stack for enough time that the `few` array
        // does not get overwritten? in other words: is
        // the compiler allowed to initialize the vector
        // in-place, in `many.vec`?
        // (if not: reserve+copy+back_inserter, would like
        // to avoid raw call to memcopy...)
        std::vector<codepoint> onstack(few, few+inl);
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

  enum BreakPptValue {
    Other                 = 1 <<  0,
    Prepend               = 1 <<  1,
    CR                    = 1 <<  2,
    LF                    = 1 <<  3,
    Control               = 1 <<  4,
    Extend                = 1 <<  5,
    Extended_Pictographic = 1 <<  6,
    Regional_Indicator    = 1 <<  7,
    SpacingMark           = 1 <<  8,
    L                     = 1 <<  9,
    V                     = 1 << 10,
    T                     = 1 << 11,
    LV                    = 1 << 12,
    LVT                   = 1 << 13,
    ZWJ                   = 1 << 14,
  };
  BreakPptValue break_ppt_value(uint32_t u) {
    // TODO: niy
    return Other;
  }

  // @thx: https://unicode.org/reports/tr29/#Grapheme_Cluster_Boundary_Rules
  void read_grapheme(std::istream_iterator<codepoint>& it, grapheme& r) {
    static std::istream_iterator<codepoint> eos;
    codepoint cp = *it;
    r.init(cp);
    ++it;

    // GB3
    if ('\r' == cp.u && eos != it && '\n' == it->u) {
      r.push_back(*it);
      ++it;
      return;
    }

#define CURR(a) (pv & (a))
#define NEXT(b) (npv & (b))
#define BOTH(a, b) (CURR(a) && NEXT(b))

    // GB4 - shortcut for some often used (eg. lf, space, full stop, ..)
    if (cp.u < 0xa0) return;

    BreakPptValue pv = break_ppt_value(cp.u);

    // GB4
    if (CURR(Control)) return;

    bool may_emoji = CURR(Extended_Pictographic);

    // GB2
    while (eos != it) {
      cp = *it;

      // GB5 - shortcut for some often used (eg. lf, space, full stop, ..)
      if (cp.u < 0xa0) return;

      BreakPptValue npv = break_ppt_value(cp.u);

      // GB5
      if (NEXT(Control)) return;

      if (
        // GB6
        BOTH(L, L|V|LV|LVT) ||
        // GB7
        BOTH(LV|V, V|T) ||
        // GB8
        BOTH(LVT|T, T)
      ) goto take;

      // GB11
      if (may_emoji && BOTH(ZWJ, Extended_Pictographic)) {
        r.push_back(cp);
        ++it;
        return;
      }

      if (
        // GB9 and GB9a
        NEXT(Extend|ZWJ|SpacingMark) ||
        // GB9b
        CURR(Prepend)
      ) goto take;

      // GB12 and GB13 - approximation, likely wrong
      if (BOTH(Regional_Indicator, Regional_Indicator)) {
        r.push_back(cp);
        ++it;
        return;
      }

      // GB999
      return;

    take:
      if (may_emoji) may_emoji = NEXT(Extend);
      pv = npv;
      r.push_back(cp);
      ++it;
    } // while !eos

#undef BOTH
#undef NEXT
#undef CURR

  } // read_grapheme

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
