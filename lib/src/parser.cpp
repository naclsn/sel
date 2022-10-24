#include <iterator>

#include <iostream>
#include <sstream>

#include "sel/parser.hpp"
#include "sel/visitors.hpp"

namespace sel {

  double NumLiteral::value() { return n; }

  std::ostream& StrLiteral::stream(std::ostream& out) { return out << s; }
  void StrLiteral::rewind() { }

  Val* LstLiteral::operator*() { return v[c]; }
  Lst& LstLiteral::operator++() { c++; return *this; }
  bool LstLiteral::end() const { return v.size()-1 <= c; }
  void LstLiteral::rewind() { c = 0; }
  size_t LstLiteral::count() { return v.size(); }

  Val* FunChain::operator()(Val* arg) {
    Val* r = arg;
    for (auto const& it : f)
      r = it->operator()(r);
    return r;
  }


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
      THEN,
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
        case Token::Type::NAME: as.name = new std::string(*t.as.name); break;
        case Token::Type::LIT_STR: as.str = new std::string(*t.as.str); break;
        case Token::Type::LIT_NUM: as.num = t.as.num;
        default: as.chr = t.as.chr;
      }
      loc = t.loc;
    }
    ~Token() {
      switch (type) {
        case Token::Type::NAME: delete as.name; as.name = nullptr; break;
        case Token::Type::LIT_STR: delete as.str; as.str = nullptr; break;
        default: ;
      }
    }
  };

  // internal
  double parseNumber(std::istream& in) {
    unsigned base = 10;
    char c;
    double r = 0;
    while (true) {
      c = in.peek();
      if (c < '0' || '9' < c) break;
      in.ignore(1);
      r = r * base + (c-'0');
    }
    if ('.' == c) {
      in.ignore(1);
      while (true) {
        c = in.peek();
        if (c < '0' || '9' < c) break;
        in.ignore(1);
        r+= ((double)(c-'0')) / base;
        base/= base;
      }
    }
    return r;
  }

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
      case Token::Type::THEN:          out << "THEN";          break;
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
      case Token::Type::THEN:
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

      case ',': t.type = Token::Type::THEN;          t.as.chr = c; in.ignore(1); break;

      case '[': t.type = Token::Type::SUB_OPEN;      t.as.chr = c; in.ignore(1); break;
      case ']': t.type = Token::Type::SUB_CLOSE;     t.as.chr = c; in.ignore(1); break;

      case '{': t.type = Token::Type::LIT_LST_OPEN;  t.as.chr = c; in.ignore(1); break;
      case '}': t.type = Token::Type::LIT_LST_CLOSE; t.as.chr = c; in.ignore(1); break;

      case '@':
      case '%':
        t.type = Token::Type::UN_OP;
        t.as.chr = c;
        in.ignore(1);
        break;

      case '+':
      case '-':
      case '.':
      case '/':
        t.type = Token::Type::BIN_OP;
        t.as.chr = c;
        in.ignore(1);
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
          t.as.num = parseNumber(in);
          break;
        }

        if ('a' <= c && c <= 'z') {
          t.type = Token::Type::NAME;
          t.as.name = new std::string();
          do {
            t.as.name->push_back(in.get());
            c = in.peek();
          } while ('a' <= c && c <= 'z');
          break;
        }
    }

    while (!in.eof() && isspace(in.peek())) in.get();
    return in;
  }

  // internal
  Val* parseAtom(Env& env, std::istream_iterator<Token>& lexer);
  Val* parseElement(Env& env, std::istream_iterator<Token>& lexer);

  // internal
  Val* parseAtom(Env& env, std::istream_iterator<Token>& lexer) {
    static std::istream_iterator<Token> eos;
    if (eos == lexer) return nullptr;

    Val* val = nullptr;
    Token t = *lexer;

    switch (t.type) {
      case Token::Type::NAME:
        val = lookup_name(env, *t.as.name);
        lexer++;
        break;

      case Token::Type::LIT_NUM:
        // val = new NumLiteral(t.as.num);
        lexer++;
        break;

      case Token::Type::LIT_STR:
        // val = new StrLiteral(std::string(*t.as.str));
        lexer++;
        break;

      case Token::Type::LIT_LST_CLOSE: lexer++; break;
      case Token::Type::LIT_LST_OPEN:
        {
          std::vector<Val*> elms;
          while (Token::Type::LIT_LST_CLOSE != (++lexer)->type) {
            elms.push_back(parseElement(env, lexer));
            if (Token::Type::LIT_LST_CLOSE != lexer->type
             && Token::Type::THEN != lexer->type)
              ; // TODO: throw syntax error: expected ',' or '}'
          }
          // val = new LstLiteral(elms);
        }
        break;

      case Token::Type::UN_OP:
        break;

      case Token::Type::BIN_OP:
        break;

      case Token::Type::SUB_CLOSE: break;
      case Token::Type::SUB_OPEN:
        {
          if (Token::Type::SUB_CLOSE == (++lexer)->type)
            ; // TODO: throw syntax error: empty sub-expression

          // single value between [] - except no one should do that >:
          val = parseElement(env, lexer);
          if (Token::Type::SUB_CLOSE == lexer->type)
            break;

          std::vector<Fun*> elms;
          do {
            elms.push_back(coerse<Fun>(val));
            if (Token::Type::THEN != lexer->type)
              break;
            val = parseElement(env, ++lexer);
          } while (val);

          if (Token::Type::SUB_CLOSE != (++lexer)->type)
            ; // TODO: throw syntax error: expected closing ']'

          // val = new FunChain(elms);
        }
        break;

      case Token::Type::THEN: break;
    }

    return val;
  }

  // internal
  Val* parseElement(Env& env, std::istream_iterator<Token>& lexer) {
    Val* val = parseAtom(env, lexer);

    while (Token::Type::THEN != lexer++->type) {
      Fun* base = coerse<Fun>(val);
      Val* arg = parseAtom(env, lexer);
      if (!arg) break;
      base->operator()(arg);
    }

    return val;
  }

  std::ostream& operator<<(std::ostream& out, App const& app) {
    return out << "hey, am an app with this many function(s): " << app.funcs.size();
  }

  std::istream& operator>>(std::istream& in, App& app) {
    std::istream_iterator<Token> lexer(in);

    while (true) {
      Val* current = parseElement(app.env, lexer);
      if (!current) break;
      app.funcs.push_back(coerse<Fun>(current));
    }

    return in;
  }

} // namespace sel
