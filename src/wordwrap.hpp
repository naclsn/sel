#ifndef SELI_WORDWRAP_HPP
#define SELI_WORDWRAP_HPP

#include <iostream>
#include <string>

template <int indent>
class wordwrapbuf : public std::streambuf {
  std::string acc;
  size_t width;
  size_t curr = 0;
  std::ostream& out;

protected:
  /// NOTE: uses the flush message to signal an end of paragraph
  int_type sync() override {
    sendword();
    curr = 0;
    return 0;
  }

  void sendword() {
    auto len = acc.length();
    if (0 == curr) {
      curr+= len;
      out << std::string(indent, ' ');
    } else if (curr+len < width) {
      curr+= 1+len;
      out << ' ';
    } else {
      curr = len;
      out << '\n' << std::string(indent, ' ');
    }
    out << acc;
    acc.clear();
  }

  int_type overflow(int_type c) override {
    if (' ' == c) sendword();
    else acc.push_back(c);
    return c;
  }

public:
  wordwrapbuf(size_t width, std::ostream& out)
    : width(width-indent)
    , out(out)
  { acc.reserve(width); }
};

template <int indent>
class wordwrap : private wordwrapbuf<indent>, public std::ostream {
public:
  wordwrap(size_t width, std::ostream& out)
    : wordwrapbuf<indent>(width, out)
    , std::ostream(this)
  { }
};

#endif // SELI_WORDWRAP_HPP
