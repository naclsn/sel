#include <iterator>

// ZZZ: some w is ip
#include <iostream>
#include <sstream>

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
      std::string* name;
      double num;
      std::string* str;
      char chr;
    } as = {0};
    Token() {}
    Token(Token const& t) {
      switch (type = t.type) {
        case Token::Type::NAME:    as.name = new std::string(*t.as.name); break;
        case Token::Type::LIT_STR: as.str = new std::string(*t.as.str);   break;
        default: ;
      }
      loc = t.loc;
    }
    ~Token() {
      switch (type) {
        case Token::Type::NAME:    delete as.name; as.name = nullptr; break;
        case Token::Type::LIT_STR: delete as.str;  as.str = nullptr;  break;
        default: ;
      }
    }
  };

  // internal
  std::ostream& operator<<(std::ostream& out, Token const& t) {
    out << "Token { .type=";
    switch (t.type) {
      case Token::Type::NAME:          out << "NAME";          break;
      case Token::Type::LIT_NUM:       out << "LIT_NUM";       break;
      case Token::Type::LIT_STR:       out << "LIT_STR";       break;
      case Token::Type::LIT_LST_OPEN:  out << "LIT_LST_OPEN";  break;
      case Token::Type::LIT_LST_CLOSE: out << "LIT_LST_CLOSE"; break;
      case Token::Type::UN_OP:         out << "UN_OP";         break;
      case Token::Type::BIN_OP:        out << "BIN_OP";        break;
      case Token::Type::SUB_OPEN:      out << "SUB_OPEN";      break;
      case Token::Type::SUB_CLOSE:     out << "SUB_CLOSE";     break;
      case Token::Type::COMMA:         out << "COMMA";         break;
    }
    out << ", ";
    switch (t.type) {
      case Token::Type::NAME:
        out << ".name=" << (t.as.name ? *t.as.name : "-nil-");
        break;

      case Token::Type::LIT_NUM:
        out << ".num=" << t.as.num;
        break;

      case Token::Type::LIT_STR:
        out << ".str=\"" << (t.as.name ? *t.as.str : "-nil-") << '"';
        break;

      case Token::Type::LIT_LST_OPEN:
      case Token::Type::LIT_LST_CLOSE:
      case Token::Type::UN_OP:
      case Token::Type::BIN_OP:
      case Token::Type::SUB_OPEN:
      case Token::Type::SUB_CLOSE:
      case Token::Type::COMMA:
        out << ".chr='" << t.as.chr << "'";
        break;
    }
    return out << ", loc=" << t.loc << " }";
  }

  // internal
  std::istream& operator>>(std::istream& in, Token& t) {
    t.loc = in.tellg();
    char c = in.peek();
    // if (in.eof()) return in;
    switch (c) {
      // case -1: // trait::eof()

      case ',': t.type = Token::Type::COMMA;         t.as.chr = in.get(); break;

      case '[': t.type = Token::Type::SUB_OPEN;      t.as.chr = in.get(); break;
      case ']': t.type = Token::Type::SUB_CLOSE;     t.as.chr = in.get(); break;

      case '{': t.type = Token::Type::LIT_LST_OPEN;  t.as.chr = in.get(); break;
      case '}': t.type = Token::Type::LIT_LST_CLOSE; t.as.chr = in.get(); break;

      case '@':
      case '%':
        t.type = Token::Type::UN_OP;
        t.as.chr = in.get();
        break;

      case '+':
      case '-':
      case '.':
      case '/':
        t.type = Token::Type::BIN_OP;
        t.as.chr = in.get();
        break;

      case ':':
        t.type = Token::Type::LIT_STR;
        t.as.str = new std::string();
        in.ignore(1);
        do {
          c = in.get();
          if (':' == c) {
            if (':' != in.peek()) break;
            in.ignore(1);
          }
          t.as.str->push_back(c);
        } while (!in.eof());
        // if (in.eof()) ..
        break;

      default:
        if ('0' <= c && c <= '9') {
          t.type = Token::Type::LIT_NUM;
          in >> t.as.num;
          break;
        }

        if ('a' <= c && c <= 'z') {
          t.type = Token::Type::NAME;
          t.as.name = new std::string();
          do {
            t.as.name->push_back(in.get());
          } while (isalpha(in.peek()));
          break;
        }
    }

    while (!in.eof() && isspace(in.peek())) in.get();
    return in;
  }

  // ZZZ: wip
  void parseApplication() {
    std::istringstream iss("split : :, map +1, join ::::");

    std::istream_iterator<Token> isi(iss);
    std::istream_iterator<Token> eos;

    std::cerr << "parsing '" << iss.str() << "'\n";
    while (eos != isi)
      std::cerr << "  read token: " << *isi++ << std::endl;
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
