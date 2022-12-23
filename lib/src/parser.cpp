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
  void LstLiteral::accept(Visitor& v) const { v.visitLstLiteral(ty, this->v); }

  Val* FunChain::operator()(Val* arg) {
    Val* r = arg;
    for (auto const& it : f)
      r = (*it)(r);
    return r;
  }
  void FunChain::accept(Visitor& v) const { v.visitFunChain(ty, f); }

  std::ostream& Input::stream(std::ostream& out) { // YYY: should it/not be reading c/c?
    // already read up to `upto`, so take from cache starting at `nowat`
    if (nowat < upto) {
      out << cache.str().substr(nowat, upto-nowat);
      nowat = upto;
      return out;
    }
    // at the end of cache, fetch for more
    char c = in->get();
    if (std::char_traits<char>::eof() != c) {
      nowat++;
      upto++;
      out.put(c);
      cache.put(c);
    }
    return out;
  }
  bool Input::end() const { return nowat == upto && in->eof(); }
  void Input::rewind() { nowat = 0; }
  std::ostream& Input::entire(std::ostream& out) {
    // not at the end, get the rest
    if (!in->eof()) {
      char buffer[4096];
      while (in->read(buffer, sizeof(buffer))) {
        upto+= 4096;
        out << buffer;
        cache << buffer;
      }
      auto end = in->gcount();
      buffer[end] = '\0';
      upto+= end;
      out << buffer;
      cache << buffer;
      nowat = upto;
      return out;
    }
    nowat = upto;
    return out << cache.str();
  }
  void Input::accept(Visitor& v) const { v.visitInput(ty); }

  Val* Output::operator()(Val* arg) {
    if (out) coerse<Str>(arg)->entire(*out);
    return nullptr;
  }
  void Output::accept(Visitor& v) const { v.visitOutput(ty); }


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
      DEF,
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
      case Token::Type::DEF:           out << "DEF";           break;
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
      case Token::Type::DEF:
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
          } else if ('\\' == c) { // YYY: not sure I like it, but this sure is convenient and more habitual
            switch (in.get()) {
              case 'a': c = '\a'; break;
              case 'b': c = '\b'; break;
              case 't': c = '\t'; break;
              case 'n': c = '\n'; break;
              case 'v': c = '\v'; break;
              case 'f': c = '\f'; break;
              case 'r': c = '\r'; break;
              case 'e': c = '\e'; break;
            }
          }
          t.as.str->push_back(c);
        } while (!in.eof());
        if (':' != c && in.eof())
          throw EOSError("matching token ':'", "- what -");
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

          if ("def" == *t.as.name) {
            t.type = Token::Type::DEF;
            delete t.as.name;
            t.as.name = nullptr;
          }

          break;
        }
    }

    while (!in.eof() && isspace(in.peek())) in.get();
    return in;
  }

  // internal
  Val* lookup(App& app, std::string const& name) {
    Val* user = app.lookup_name_user(name);
    if (user) return user;
    return lookup_name(name);
  }

  // internal
  Val* parseAtom(App& app, std::istream_iterator<Token>& lexer);
  Val* parseElement(App& app, std::istream_iterator<Token>& lexer);

  // internal
  Val* parseAtom(App& app, std::istream_iterator<Token>& lexer) {
    TRACE(parseAtom, *lexer);
    static std::istream_iterator<Token> eos;
    if (eos == lexer) throw EOSError("atom", "- what -");

    Val* val = nullptr;
    Token t = *lexer;

    switch (t.type) {
      case Token::Type::NAME:
        val = lookup(app, *t.as.name);
        if (!val) throw NameError(*t.as.name, "- what -");
        lexer++;
        break;

      case Token::Type::DEF:
        {
          t = *++lexer;
          if (Token::Type::NAME != t.type)
            throw ParseError("name", std::string("got unexpected tokens ") + (char)((char)t.type+'0'), "- what -");
          if (lookup(app, *t.as.name))
            throw ParseError("new name", "got known name " + *t.as.name, "- what -");
          lexer++;
          val = parseAtom(app, lexer);
          app.define_name_user(*t.as.name, val);
        }
        break;

      case Token::Type::LIT_NUM:
        val = new NumLiteral(t.as.num);
        lexer++;
        break;

      case Token::Type::LIT_STR:
        val = new StrLiteral(std::string(*t.as.str));
        lexer++;
        break;

      case Token::Type::LIT_LST_CLOSE: lexer++; break; // YYY: unreachable?
      case Token::Type::LIT_LST_OPEN:
        {
          std::vector<Val*> elms;
          while (Token::Type::LIT_LST_CLOSE != (++lexer)->type) {
            elms.push_back(parseElement(app, lexer));
            if (Token::Type::LIT_LST_CLOSE != lexer->type
             && Token::Type::THEN != lexer->type)
              throw ParseError("token ',' or matching token '}'", std::string("got unexpected tokens ") + (char)((char)t.type+'0'), "- what -");
          }

          // TODO: for the type, need to know if the list
          // literal should be a tuple or an actual list
          // in both cases, may need to get the shortest
          // repeating pattern, eg. `{:coucou:, 42,
          // :blabla:, 12}` could be:
          //   - `(Str, Num, Str, Num)`
          //   - `[Str, Num]`

          val = new LstLiteral(elms);
        }
        break;

      case Token::Type::UN_OP: // TODO: will have a lookup_unop
        switch (t.as.chr) {
          // case '@': val = lookup_name(""); break;
          case '%': val = lookup_name("flip"); break;
          // default unreachable
        }
        {
          Val* arg;
          Token t = *++lexer;
          if (Token::Type::BIN_OP == t.type) { // ZZZ: yeah, code dup (as if that was the only problem)
            switch (t.as.chr) {
              case '+': arg = lookup_name("add"); break;
              case '-': arg = lookup_name("sub"); break;
              case '.': arg = lookup_name("mul"); break;
              case '/': arg = lookup_name("div"); break;
              // default unreachable
            }
            arg = (*(Fun*)lookup_name("flip"))(arg);
            lexer++;
          } else arg = parseAtom(app, lexer);
          val = (*(Fun*)val)(arg);
        }
        break;

      case Token::Type::BIN_OP: // TODO: will have a lookup_unop
        switch (t.as.chr) {
          case '+': val = lookup_name("add"); break;
          case '-': val = lookup_name("sub"); break;
          case '.': val = lookup_name("mul"); break;
          case '/': val = lookup_name("div"); break;
          // default unreachable
        }
        val = (*(Fun*)(*(Fun*)lookup_name("flip"))(val))(parseAtom(app, ++lexer));
        break;

      case Token::Type::SUB_CLOSE: lexer++; break; // YYY: unreachable?
      case Token::Type::SUB_OPEN:
        {
          if (Token::Type::SUB_CLOSE == (++lexer)->type)
            throw ParseError("element", "got empty sub-expression", "- what -");

          // early break: single value between []
          val = parseElement(app, lexer);
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
            val = parseElement(app, ++lexer);
          } while (true);

          if (Token::Type::SUB_CLOSE != lexer++->type)
            throw ParseError("matching token ']'", std::string("got unexpected tokens ") + (char)((char)t.type+'0'), "- what -");

          val = new FunChain(elms);
        }
        break;

      case Token::Type::THEN: break;
    }

    if (!val) throw ParseError("atom", std::string("got unexpected tokens ") + (char)((char)t.type+'0'), "- what -");
    return val;
  }

  // internal
  Val* parseElement(App& app, std::istream_iterator<Token>& lexer) {
    TRACE(parseElement, *lexer);
    static std::istream_iterator<Token> eos;
    if (eos == lexer) throw EOSError("element", "- what -");
    Val* val = parseAtom(app, lexer);

    while (Token::Type::THEN != lexer->type
        && Token::Type::LIT_LST_CLOSE != lexer->type
        && Token::Type::SUB_CLOSE != lexer->type
        && eos != lexer) {
      Fun* base = coerse<Fun>(val);
      Val* arg = parseAtom(app, lexer);
      val = base->operator()(arg);
    }

    return val;
  }

  Val* App::lookup_name_user(std::string const& name) {
    return user[name];
  }

  void App::define_name_user(std::string const& name, Val* v) {
    user[name] = v;
  }

  void App::run(std::istream& in, std::ostream& out) {
    // if (!fin || !fout) throw BaseError("uninitialized or malformed application");

    fin->setIn(&in);
    fout->setOut(&out);

    Val* r = fin;
    for (auto const& it : funcs)
      r = (*it)(r);
    (*fout)(r);

    fin->setIn(nullptr);
    fout->setOut(nullptr);
  }

  void App::repr(std::ostream& out, VisRepr::ReprCx cx) const {
    VisRepr repr(out, cx);
    repr(*fin);
    for (auto const& it : funcs)
      repr(*it);
    repr(*fout);
  }

  std::ostream& operator<<(std::ostream& out, App const& app) {
    // TODO: todo
    out << "hey, am an app with this many function(s): " << app.funcs.size();
    for (auto const& it : app.funcs)
      out << "\n\t" << it->type();
    return out;
  }

  std::istream& operator>>(std::istream& in, App& app) {
    std::istream_iterator<Token> lexer(in);
    static std::istream_iterator<Token> eos;
    if (eos == lexer) throw EOSError("script", "- what -");

    app.fin = new Input();

    do {
      Val* current = parseElement(app, lexer);
      app.funcs.push_back(coerse<Fun>(current));
    } while (Token::Type::THEN == lexer->type && eos != ++lexer);

    app.fout = new Output();

    return in;
  }

} // namespace sel
