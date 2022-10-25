#include <iterator>
#include <iostream>
#include <sstream>

#include "sel/errors.hpp"
#include "sel/parser.hpp"
#include "sel/visitors.hpp"

namespace sel {

  double NumLiteral::value() { return n; }
  void NumLiteral::accept(Visitor& v) const { v.visitNumLiteral(ty, n); }

  std::ostream& StrLiteral::stream(std::ostream& out) { read = true; return out << s; }
  bool StrLiteral::end() const { return read; }
  void StrLiteral::rewind() { read = false; }
  std::ostream& StrLiteral::entire(std::ostream& out) { read = true; return out << s; }
  void StrLiteral::accept(Visitor& v) const { v.visitStrLiteral(ty, s); }

  Val* LstLiteral::operator*() { return v[c]; }
  Lst& LstLiteral::operator++() { c++; return *this; }
  bool LstLiteral::end() const { return v.size()-1 <= c; }
  void LstLiteral::rewind() { c = 0; }
  size_t LstLiteral::count() { return v.size(); }
  void LstLiteral::accept(Visitor& v) const { v.visitLstLiteral(ty, this->v); }

  Val* FunChain::operator()(Val* arg) {
    Val* r = arg;
    for (auto const& it : f)
      r = (*it)(r);
    return r;
  }
  void FunChain::accept(Visitor& v) const { v.visitFunChain(ty, f); }

  Val* Stdin::operator()(Val* _arg) {
    std::string line("");
    if (in) std::getline(*in, line);
    return new StrLiteral(env, line);
  }
  void Stdin::accept(Visitor& v) const { v.visitStdin(ty); }

  Val* Stdout::operator()(Val* arg) {
    if (out) coerse<Str>(arg)->entire(*out) << std::endl;
    return new Nil(env);
  }
  void Stdout::accept(Visitor& v) const { v.visitStdout(ty); }


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
    char c = in.peek();
    if (in.eof()) return in;
    t.loc = in.tellg();
    switch (c) {
      // case -1: // trait::eof()

      case ',': t.type = Token::Type::THEN;          t.as.chr = c; in.ignore(1); break;

      case '[': t.type = Token::Type::SUB_OPEN;      t.as.chr = c; in.ignore(1); break;
      case ']': t.type = Token::Type::SUB_CLOSE;     t.as.chr = c; in.ignore(1); break;

      case '{': t.type = Token::Type::LIT_LST_OPEN;  t.as.chr = c; in.ignore(1); break;
      case '}': t.type = Token::Type::LIT_LST_CLOSE; t.as.chr = c; in.ignore(1); break;

      // case '@':
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
        // TODO: if (in.eof()) ..
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
    TRACE(parseAtom, *lexer);
    static std::istream_iterator<Token> eos;
    if (eos == lexer) throw EOSError("atom", "- what -");

    Val* val = nullptr;
    Token t = *lexer;

    switch (t.type) {
      case Token::Type::NAME:
        val = lookup_name(env, *t.as.name);
        if (!val) throw NameError(*t.as.name, "- what -");
        lexer++;
        break;

      case Token::Type::LIT_NUM:
        val = new NumLiteral(env, t.as.num);
        lexer++;
        break;

      case Token::Type::LIT_STR:
        val = new StrLiteral(env, std::string(*t.as.str));
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

          // TODO: for the type, need to know if the list
          // literal should be a tuple or an actual list
          // in both cases, may need to get the shortest
          // repeating pattern, eg. `{:coucou:, 42,
          // :blabla:, 12}` could be:
          //   - `(Str, Num, Str, Num)`
          //   - `[Str, Num]`

          val = new LstLiteral(env, elms);
        }
        break;

      case Token::Type::UN_OP:
        throw NIYError("unary syntax", "- what -");
        break;

      case Token::Type::BIN_OP:
        throw NIYError("binary syntax", "- what -");
        break;

      case Token::Type::SUB_CLOSE: break;
      case Token::Type::SUB_OPEN:
        {
          if (Token::Type::SUB_CLOSE == (++lexer)->type)
            throw ParseError("element", "got empty sub-expression", "- what -");

          // early break: single value between []
          val = parseElement(env, lexer);
          if (Token::Type::SUB_CLOSE == lexer->type) {
            lexer++;
            break;
          }

          if (Token::Type::THEN != lexer->type)
            throw ParseError("token ',' or matching token ']'", std::string("got unexpected tokens ") + (char)((char)t.type+'0'), "- what -");

          std::vector<Fun*> elms;
          do {
            elms.push_back(coerse<Fun>(val));
            if (Token::Type::THEN != lexer->type && eos != lexer)
              break;
            val = parseElement(env, ++lexer);
          } while (true);

          if (Token::Type::SUB_CLOSE != lexer++->type)
            throw ParseError("matching token ']'", std::string("got unexpected tokens ") + (char)((char)t.type+'0'), "- what -");

          val = new FunChain(env, elms);
        }
        break;

      case Token::Type::THEN: break;
    }

    if (!val) throw ParseError("atom", std::string("got unexpected tokens ") + (char)((char)t.type+'0'), "- what -");
    return val;
  }

  // internal
  Val* parseElement(Env& env, std::istream_iterator<Token>& lexer) {
    TRACE(parseElement, *lexer);
    static std::istream_iterator<Token> eos;
    if (eos == lexer) throw EOSError("element", "- what -");
    Val* val = parseAtom(env, lexer);

    while (Token::Type::THEN != lexer->type
        && Token::Type::LIT_LST_CLOSE != lexer->type
        && Token::Type::SUB_CLOSE != lexer->type
        && eos != lexer) {
      Fun* base = coerse<Fun>(val);
      Val* arg = parseAtom(env, lexer);
      val = base->operator()(arg);
    }

    return val;
  }

  void App::run(std::istream& in, std::ostream& out) {
    // if (!fin || !fout) throw BaseError("uninitialized or malformed application");

    fin->setIn(&in);
    fout->setOut(&out);

    Val* r = new Nil(env);
    for (auto const& it : funcs)
      r = (*it)(r);

    fin->setIn(nullptr);
    fout->setOut(nullptr);
  }

  void App::runToEnd(std::istream& in, std::ostream& out) {
    // if (!fin || !fout) throw BaseError("uninitialized or malformed application");

    fin->setIn(&in);
    fout->setOut(&out);

    Val* r = new Nil(env);
    while (in.peek() && !in.eof())
      for (auto const& it : funcs)
        r = (*it)(r);

    fin->setIn(nullptr);
    fout->setOut(nullptr);
  }

  // XXX: that's not quite it, but close
  void App::recurse() {
    // if (!fin->in || !fout->out) throw BaseError("cannot recurse, not in a run");

    Val* r = new Nil(env);
    for (auto const& it : funcs)
      r = (*it)(r);
  }

  void App::repr(std::ostream& out, VisRepr::ReprCx cx) const {
    VisRepr repr(out, cx);
    for (auto const& it : funcs)
      repr(*it);
  }

  std::ostream& operator<<(std::ostream& out, App const& app) {
    out << "hey, am an app with this many function(s): " << app.funcs.size();
    for (auto const& it : app.funcs)
      out << "\n\t" << it->type();
    return out;
  }

  std::istream& operator>>(std::istream& in, App& app) {
    std::istream_iterator<Token> lexer(in);
    static std::istream_iterator<Token> eos;
    if (eos == lexer) throw EOSError("script", "- what -");

    app.funcs.push_back(app.fin = new Stdin(app.env));

    do {
      Val* current = parseElement(app.env, lexer);
      app.funcs.push_back(coerse<Fun>(current));
    } while (Token::Type::THEN == lexer->type && eos != ++lexer);

    app.funcs.push_back(app.fout = new Stdout(app.env));

    return in;
  }

} // namespace sel
