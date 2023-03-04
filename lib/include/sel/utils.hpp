#ifndef SEL_UTILS_HPP
#define SEL_UTILS_HPP

#include <iostream>

// @thx: https://renenyffenegger.ch/notes/development/languages/C-C-plus-plus/preprocessor/macros/__VA_ARGS__/count-arguments
#define __A11(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, ...) a11
#define __VA_COUNT(...) __A11(dum, ## __VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

namespace sel {

  struct Val;

  /**
   * For use in printing a pointer.
   */
  struct raw {
    void const* ptr;
    raw(void const* ptr): ptr(ptr) { }
  };
  std::ostream& operator<<(std::ostream& out, raw ptr);

  /**
   * For use in representing a value.
   */
  struct repr {
    Val const& val;
    bool single_line;
    inline repr(Val const& val, bool single_line=true): val(val), single_line(single_line) { }
  };
  std::ostream& operator<<(std::ostream& out, repr me);

  /**
   * For use in representing text.
   */
  struct quoted {
    std::string const& str;
    bool do_col;
    bool put_quo;
    inline quoted(std::string const& str): str(str), do_col(false), put_quo(true) { }
    inline quoted(std::string const& str, bool do_col, bool put_quo): str(str), do_col(do_col), put_quo(put_quo) { }
  };
  std::ostream& operator<<(std::ostream& out, quoted q);

  /**
   * Seems 'boolalpha' was a more important thing
   * than being able to do indentation out of the box.
   * (this the 'buffer', see `indented`)
   */
  class indentedbuf : public std::streambuf {
    char const* style;
    size_t count;

    unsigned cur = 0;
    bool pending = true;

    std::ostream& out;

  protected:
    int_type overflow(int_type c) override;

  public:
    indentedbuf(char const* style, std::ostream& out)
      : style(style)
      , count(std::char_traits<char_type>::length(style))
      , out(out)
    { }

    void indent() { cur++; }
    void dedent() { cur--; }
  };

  /**
   * Seems 'boolalpha' was a more important thing
   * than being able to do indentation out of the box.
   */
  class indented : private indentedbuf, public std::ostream {
  public:
    indented(char const* style, std::ostream& out)
      : indentedbuf(style, out)
      , std::ostream(this)
    { }

    indented& indent() { indentedbuf::indent(); return *this; }
    indented& dedent() { indentedbuf::dedent(); return *this; }
  };

}

#endif // SEL_UTILS_HPP
