#include <ostream>
#include <istream>
#include <string>

#include "sel/parser.hpp"
#include "sel/visitors.hpp"

namespace sel {

  void NumLiteral::accept(Visitor& v) const { v.visitNumLiteral(type(), n); }

  // internal
  struct Token {
    size_t loc;
    enum class Type {
      NAME,
      LIT_NUM,
      LIT_STR,
      LIT_LST_OPEN,
      LIT_LST_CLOSE,
      UN_OP,
      BIN_OP,
      SUB_OPEN,
      SUB_CLOSE,
      COMMA,
    } type;
    union {
      std::string name;
      double num;
      std::string str;
      char chr;
    } as;
  };

  // internal
  std::istream& operator>>(std::istream& in, Token& t) {
    char c = in.get();
    // if (in.eof()) return in;
    switch (c) {
      // case -1: // trait::eof()

      case ',': t.type = Token::Type::COMMA;         t.as.chr = c; break;

      case '[': t.type = Token::Type::SUB_OPEN;      t.as.chr = c; break;
      case ']': t.type = Token::Type::SUB_CLOSE;     t.as.chr = c; break;

      case '{': t.type = Token::Type::LIT_LST_OPEN;  t.as.chr = c; break;
      case '}': t.type = Token::Type::LIT_LST_CLOSE; t.as.chr = c; break;

      case '@':
      case '%':
        t.type = Token::Type::UN_OP;
        t.as.chr = c;
        break;

      case '+':
      case '-':
      case '.':
      case '/':
        t.type = Token::Type::BIN_OP;
        t.as.chr = c;
        break;

      case ':':
        t.type = Token::Type::LIT_STR;
        t.as.str = c;
        while (!in.eof()) {
          c = in.get();
          if (':' == c) {
            if (':' != in.peek()) break;
            in.ignore(1);
          }
          t.as.str.push_back(c);
        }
        break;

      default:
        if ('0' <= c && c <= '9') {
          t.type = Token::Type::LIT_NUM;
          in >> t.as.num;
          break;
        }

        if ('a' <= c && c <= 'z') {
          t.type = Token::Type::NAME;
          t.as.name = c;
          while (isalpha(in.peek()))
            t.as.name.push_back(in.get());
          break;
        }
    }

    while (!in.eof() && isspace(in.peek())) in.get();
    return in;
  }

  std::ostream& operator<<(std::ostream& out, Application const& app) {
    throw "TODO: operator<< for Application\n";
    return out;
  }

  std::istream& operator>>(std::istream& in, Application& app) {
    throw "TODO: operator>> for Application\n";
    return in;
  }

} // namespace sel
