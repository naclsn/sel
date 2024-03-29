#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>

#include "sel/application.hpp"
#include "sel/builtins.hpp"
#include "sel/errors.hpp"
#include "sel/parser.hpp"
#include "sel/utils.hpp"
#include "sel/visitors.hpp"

using namespace std;

namespace sel {

  double NumLiteral::value() { return n; }
  unique_ptr<Val> NumLiteral::copy() const { return make_unique<NumLiteral>(n); }

  ostream& StrLiteral::stream(ostream& out) { read = true; return out << s; }
  bool StrLiteral::end() { return read || s.empty(); }
  ostream& StrLiteral::entire(ostream& out) { read = true; return out << s; }
  unique_ptr<Val> StrLiteral::copy() const { return make_unique<StrLiteral>(string(s)); }

  unique_ptr<Val> LstLiteral::operator++() { return c < v.size() ? move(v[c++]) : nullptr; }
  unique_ptr<Val> LstLiteral::copy() const {
    vector<unique_ptr<Val>> w;
    w.reserve(v.size());
    for (auto const& it : v)
      w.push_back(it->copy());
    return val_cast<Val>(make_unique<LstLiteral>(move(w), vector<Type>(ty.has())));
  }

  unique_ptr<Val> FunChain::operator()(unique_ptr<Val> arg) {
    unique_ptr<Val> r = move(arg);
    for (auto& it : f)
      r = (*it)(move(r));
    return r;
  }
  unique_ptr<Val> FunChain::copy() const {
    vector<unique_ptr<Fun>> g;
    g.reserve(f.size());
    for (auto const& it : f)
      g.push_back(val_cast<Fun>(it->copy()));
    return val_cast<Val>(make_unique<FunChain>(move(g)));
  }

  double NumDefine::value() { return v->value(); }
  unique_ptr<Val> NumDefine::copy() const { return val_cast<Val>(make_unique<NumDefine>(name, doc, val_cast<Num>(v->copy()))); }

  ostream& StrDefine::stream(ostream& out) { return v->stream(out); }
  bool StrDefine::end() { return v->end(); }
  ostream& StrDefine::entire(ostream& out) { return v->entire(out); }
  unique_ptr<Val> StrDefine::copy() const { return val_cast<Val>(make_unique<StrDefine>(name, doc, val_cast<Str>(v->copy()))); }

  unique_ptr<Val> LstDefine::operator++() { return ++(*v); }
  unique_ptr<Val> LstDefine::copy() const { return val_cast<Val>(make_unique<LstDefine>(name, doc, val_cast<Lst>(v->copy()))); }

  unique_ptr<Val> FunDefine::operator()(unique_ptr<Val> arg) { return (*v)(move(arg)); }
  unique_ptr<Val> FunDefine::copy() const { return val_cast<Val>(make_unique<FunDefine>(name, doc, val_cast<Fun>(v->copy()))); }

  ostream& Input::stream(ostream& out) {
    // already read up to `upto`, so take from cache starting at `nowat`
    if (nowat < buffer->upto) {
      out << buffer->cache.str().substr(nowat, buffer->upto-nowat);
      nowat = buffer->upto;
      return out;
    }
    // at the end of cache, fetch for more
    auto const avail = buffer->in.rdbuf()->in_avail();
    //if (!avail) return out;
    if (0 < avail) {
      // try extrating multiple at once (usl/unreachable if `sync_with_stdio(true)`)
      char small[avail];
      buffer->in.read(small, avail);
      nowat+= avail;
      buffer->upto+= avail;
      out.write(small, avail);
    } else {
      // can only get the next one; forward if not eof
      char c = buffer->in.get();
      if (char_traits<char>::eof() != c) {
        nowat++;
        buffer->upto++;
        out.put(c);
        buffer->cache.put(c);
      }
    } // if (0 < avail)
    return out;
  }
  bool Input::end() {
    return nowat == buffer->upto && (buffer->in.eof() || istream::traits_type::eof() == buffer->in.peek());
  }
  ostream& Input::entire(ostream& out) {
    // not at the end, get the rest
    if (!buffer->in.eof()) {
      char tmp[4096];
      while (buffer->in.read(tmp, sizeof(tmp))) {
        buffer->upto+= 4096;
        out << tmp;
        buffer->cache << tmp;
      }
      auto end = buffer->in.gcount();
      tmp[end] = '\0';
      buffer->upto+= end;
      out << tmp;
      buffer->cache << tmp;
      nowat = buffer->upto;
      return out;
    }
    nowat = buffer->upto;
    return out << buffer->cache.str();
  }
  unique_ptr<Val> Input::copy() const { return unique_ptr<Val>(new Input(*this)); }

  // internal
  struct Token {
    size_t loc;
    size_t len;
    enum class Type {
      END,
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
      PASS,
      DEF,
    } type;
    union {
      string* name;
      double num;
      string* str;
      char chr;
    } as = {0};
    Token() {}
    Token(Token const& t) {
      switch (type = t.type) {
        case Token::Type::NAME: as.name = new string(*t.as.name); break;
        case Token::Type::LIT_STR: as.str = new string(*t.as.str); break;
        case Token::Type::LIT_NUM: as.num = t.as.num;
        default: as.chr = t.as.chr;
      }
      loc = t.loc;
      len = t.len;
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
  double parseNumber(istream& in, size_t* len) {
    unsigned base = 10;
    char c;
    double r = 0;
    while (true) {
      c = in.peek();
      (*len)++;
      if (c < '0' || '9' < c) break;
      in.ignore(1);
      r = r * base + (c-'0');
    }
    if ('.' == c) {
      in.ignore(1);
      while (true) {
        c = in.peek();
        (*len)++;
        if (c < '0' || '9' < c) break;
        in.ignore(1);
        r+= ((double)(c-'0')) / base;
        base/= base;
      }
    }
    return r;
  }

  // internal
  ostream& reprToken(ostream& out, Token const& t) {
    out << "Token { .type=";
    switch (t.type) {
      case Token::Type::END:           out << "END";           break;
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
      case Token::Type::PASS:          out << "PASS";          break;
      case Token::Type::DEF:           out << "DEF";           break;
    }
    out << ", ";
    switch (t.type) {
      case Token::Type::END:
        break;

      case Token::Type::NAME:
        out << ".name=" << (t.as.name ? *t.as.name : "-nil-");
        break;

      case Token::Type::LIT_NUM:
        out << ".num=" << t.as.num;
        break;

      case Token::Type::LIT_STR:
        out << ".str="; if (t.as.str) out << utils::quoted(*t.as.str); else out << "-nil-";
        break;

      case Token::Type::LIT_LST_OPEN:
      case Token::Type::LIT_LST_CLOSE:
      case Token::Type::UN_OP:
      case Token::Type::BIN_OP:
      case Token::Type::SUB_OPEN:
      case Token::Type::SUB_CLOSE:
      case Token::Type::THEN:
      case Token::Type::PASS:
        out << ".chr='" << t.as.chr << "'";
        break;

      case Token::Type::DEF:
        break;
    }
    return out << ", loc=" << t.loc << " }";
  }

  // internal
  ostream& operator<<(ostream& out, Token const& t) {
    switch (t.type) {
      case Token::Type::END:
        out << "end of script";
        break;

      case Token::Type::NAME:
        out << "name '" << *t.as.name << "'";
        break;

      case Token::Type::LIT_NUM:
        // ankward: don't have the textual representation
        // anymore, would need to re-extract it from `loc`
        // in the input source...
        out << "literal number of value " << t.as.num;
        break;

      case Token::Type::LIT_STR:
        out << "literal string " << utils::quoted(*t.as.str);
        break;

      case Token::Type::LIT_LST_OPEN:
      case Token::Type::LIT_LST_CLOSE:
      case Token::Type::UN_OP:
      case Token::Type::BIN_OP:
      case Token::Type::SUB_OPEN:
      case Token::Type::SUB_CLOSE:
      case Token::Type::THEN:
      case Token::Type::PASS:
        out << "token '" << t.as.chr << "'";
        break;

      case Token::Type::DEF:
        out << "special name \"def\"";
        break;
    }
    return out;
  }

  // internal
  void expected(char const* should, Token const& got) {
    ostringstream oss;
    oss << "expected " << should << " but got " << got << " instead";
    throw ParseError(oss.str(), got.loc, got.len);
  }
  void expectedMatching(Token const& open, Token const& got) {
    ostringstream oss;
    oss << "expected closing match for " << open << " but got " << got << " instead";
    throw ParseError(oss.str(), open.loc, got.loc-open.loc);
  }
  void expectedContinuation(char const* whiledotdotdot, Token const& somelasttoken) {
    ostringstream oss;
    oss << "reached end of script while " << whiledotdotdot;
    throw ParseError(oss.str(), somelasttoken.loc, somelasttoken.len);
  }
  void _fakeThrowErr(Token const& hl) {
    ostringstream oss;
    oss << "this is a fake error for testing purpose, token: " << hl;
    throw ParseError(oss.str(), hl.loc, hl.len);
  }

  // internal
  istream& operator>>(istream& in, Token& t) {
    char c = in.peek();
    if (in.eof()) {
      t.type = Token::Type::END;
      return in;
    }
    if ('#' == c) {
      in.ignore(numeric_limits<streamsize>::max(), '\n');
      return in >> t;
    }
    if ('\t' == c || '\n' == c || '\r' == c || ' ' == c) { // YYY: what is considered 'whitespace' by C++?
      in.ignore(1);
      return in >> t;
    }

    t.loc = in.tellg(); // YYY: no (no?)
    t.len = 0;
    switch (c) {
      // case -1: // trait::eof()

      case ',': t.type = Token::Type::THEN;          t.as.chr = c; in.ignore(1); t.len = 1; break;
      case ';': t.type = Token::Type::PASS;          t.as.chr = c; in.ignore(1); t.len = 1; break;

      case '[': t.type = Token::Type::SUB_OPEN;      t.as.chr = c; in.ignore(1); t.len = 1; break;
      case ']': t.type = Token::Type::SUB_CLOSE;     t.as.chr = c; in.ignore(1); t.len = 1; break;

      case '{': t.type = Token::Type::LIT_LST_OPEN;  t.as.chr = c; in.ignore(1); t.len = 1; break;
      case '}': t.type = Token::Type::LIT_LST_CLOSE; t.as.chr = c; in.ignore(1); t.len = 1; break;

      // case '@':
      case '%':
        t.type = Token::Type::UN_OP;
        t.as.chr = c;
        in.ignore(1);
        t.len = 1;
        break;

      case '+':
      case '-':
      case '.':
      case '/':
      case '_':
        t.type = Token::Type::BIN_OP;
        t.as.chr = c;
        in.ignore(1);
        t.len = 1;
        break;

      case ':':
        t.type = Token::Type::LIT_STR;
        t.as.str = new string();
        in.ignore(1);
        t.len++;
        do {
          c = in.get();
          t.len++;
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
        if (':' != c && in.eof()) expectedContinuation("scanning string literal", t);
        break;

      default:
        if ('0' <= c && c <= '9') {
          t.type = Token::Type::LIT_NUM;
          t.as.num = parseNumber(in, &t.len);
          break;
        }

        if ('a' <= c && c <= 'z') {
          t.type = Token::Type::NAME;
          t.as.name = new string();
          do {
            t.as.name->push_back(in.get());
            t.len++;
            c = in.peek();
          } while ('a' <= c && c <= 'z');

          if ("def" == *t.as.name) {
            t.type = Token::Type::DEF;
            delete t.as.name;
            t.as.name = nullptr;
          }

          break;
        }

        throw ParseError(string("unexpected character '") + c + "' (" + to_string((int)c) + ")", t.loc, 1);
    } // switch c

    while (!in.eof() && isspace(in.peek())) in.get();
    return in;
  }

  // internal
  unique_ptr<Val> lookup(App& app, string const& name) {
    unique_ptr<Val> user = app.lookup_name_user(name);
    if (user) return user;
    return lookup_name(name.c_str());
  }
  unique_ptr<Val> lookup_unary(App& app, Token const& t) {
    switch (t.as.chr) {
      // case '@': val = static_lookup_name(..); break;
      case '%': return static_lookup_name(flip); break; // YYY: flips cancel out
    }
    // default unreachable
    throw TypeError(string("unreachable in lookup_unary for character '") + t.as.chr + '\'');
  }
  unique_ptr<Val> lookup_binary(App& app, Token const& t) {
    switch (t.as.chr) {
      case '+': return static_lookup_name(add); break;
      case '-': return static_lookup_name(sub); break;
      case '.': return static_lookup_name(mul); break;
      case '/': return static_lookup_name(div); break;
      case '_': return static_lookup_name(index); break;
    }
    // default unreachable
    throw TypeError(string("unreachable in lookup_binary for character '") + t.as.chr + '\'');
  }

  // internal
  static istream_iterator<Token> eos;
  unique_ptr<Val> parseAtom(App& app, istream_iterator<Token>& lexer);
  unique_ptr<Val> parseElement(App& app, istream_iterator<Token>& lexer);
  unique_ptr<Val> parseScript(App& app, istream_iterator<Token>& lexer);

  // internal
  unique_ptr<Val> parseAtom(App& app, istream_iterator<Token>& lexer) {
    if (eos == lexer) expectedContinuation("scanning atom", *lexer);

    Token t = *lexer;
    switch (t.type) {
      case Token::Type::END:
        break;

      case Token::Type::NAME: {
          auto val = lookup(app, *t.as.name);
          if (!val) throw ParseError("unknown name '" + *t.as.name + "'", t.loc, t.len);
          ++lexer;
          return val;
        }

      case Token::Type::LIT_NUM: {
          auto val = make_unique<NumLiteral>(t.as.num);
          ++lexer;
          return val;
        }

      case Token::Type::LIT_STR: {
          auto val = make_unique<StrLiteral>(move(*t.as.str));
          ++lexer;
          return val;
        }

      case Token::Type::LIT_LST_OPEN: {
          vector<unique_ptr<Val>> elms;
          while (Token::Type::LIT_LST_CLOSE != (++lexer)->type) {
            elms.push_back(parseElement(app, lexer));
            if (Token::Type::LIT_LST_CLOSE == lexer->type) break;
            if (eos == lexer) expectedContinuation("scanning list literal", *lexer);
            if (Token::Type::THEN != lexer->type) expectedMatching(t, *lexer);
          }

          // TODO: for the type, need to know if the list
          // literal should be a tuple or an actual list
          // in both cases, may need to get the shortest
          // repeating pattern, eg. `{:coucou:, 42,
          // :blabla:, 12}` could be:
          //   - `(Str, Num, Str, Num)`
          //   - `[Str, Num]`
          // will probably only consider tuples up to 2~3
          // len, with len 1 also defaulting to list
          // could also apply some rules accounting for
          // auto cohersion (eg. list of mixed Num&Str ->
          // [Str]) / look at surrounding (this would have
          // to be done at element-s level...)

          auto val = val_cast<Val>(make_unique<LstLiteral>(move(elms)));
          ++lexer;
          return val;
        }

      case Token::Type::UN_OP: {
          auto val = lookup_unary(app, t);
          Token t = *++lexer;
          auto arg = Token::Type::BIN_OP == t.type
            ? (++lexer, (*static_lookup_name(flip))(lookup_binary(app, t)))
            : parseAtom(app, lexer);
          if (!arg) expected("atom", *lexer);
          val = (*(Fun*)val.get())(move(arg));
          return val;
        }

      case Token::Type::BIN_OP: {
          // it parses the argument first, the op is on the stack for below
          unique_ptr<Val> arg = parseAtom(app, ++lexer);
          if (!arg) expected("atom", *lexer);
          return (*(Fun*)(*static_lookup_name(flip))(lookup_binary(app, t)).get())(move(arg));
        }

      case Token::Type::SUB_OPEN: {
          if (Token::Type::SUB_CLOSE == (++lexer)->type)
            throw ParseError("expected element, got empty sub-expression", t.loc, lexer->loc-t.loc);

          auto val = parseScript(app, lexer);
          if (!val) expectedMatching(t, *lexer);

          if (Token::Type::SUB_CLOSE != lexer->type) {
            if (eos == lexer) expectedContinuation("scanning sub-script element", *lexer);
            expectedMatching(t, *lexer);
          }
          ++lexer;
          return val;
        }

      // YYY: (probly) unreachables
      case Token::Type::DEF:
      case Token::Type::LIT_LST_CLOSE:
      case Token::Type::SUB_CLOSE:
      case Token::Type::THEN:
      case Token::Type::PASS:
        break;
    }

    return nullptr;
  } // parseAtom

  // internal
  unique_ptr<Val> specialFunctionDef(App& app, istream_iterator<Token>& lexer) {
    Token t_name = *++lexer;
    if (Token::Type::NAME != t_name.type) expected("name", t_name);

    Token t_help = *++lexer;
    if (Token::Type::LIT_STR != t_help.type) expected(("docstring for '" + *t_name.as.name + "'").c_str(), t_help);

    if (lookup(app, *t_name.as.name)) {
      //throw TypeError
      throw ParseError("cannot redefine already known name '" + *t_name.as.name + "'", t_name.loc, t_name.len);
    }

    ++lexer;
    auto val = parseAtom(app, lexer);
    if (!val) expected("atom", *lexer);

    // trim, join, squeeze..
    string::size_type head = t_help.as.str->find_first_not_of("\t\n\r "), tail;
    bool blank = string::npos == head;
    string clean;
    if (!blank) {
      clean.reserve(t_help.as.str->length());
      while (string::npos != head) {
        tail = t_help.as.str->find_first_of("\t\n\r ", head);
        clean.append(*t_help.as.str, head, string::npos == tail ? tail : tail-head).push_back(' ');
        head = t_help.as.str->find_first_not_of("\t\n\r ", tail);
      }
      clean.pop_back();
      clean.shrink_to_fit();
    }

    switch (val->type().base()) {
      case Ty::NUM: val = val_cast<Val>(make_unique<NumDefine>(*t_name.as.name, clean, val_cast<Num>(move(val)))); break;
      case Ty::STR: val = val_cast<Val>(make_unique<StrDefine>(*t_name.as.name, clean, val_cast<Str>(move(val)))); break;
      case Ty::LST: val = val_cast<Val>(make_unique<LstDefine>(*t_name.as.name, clean, val_cast<Lst>(move(val)))); break;
      case Ty::FUN: val = val_cast<Val>(make_unique<FunDefine>(*t_name.as.name, clean, val_cast<Fun>(move(val)))); break;
      default: {
        ostringstream oss;
        throw TypeError((oss << "unexpected type in def expression: '" << val->type() << "'", oss.str()));
      }
    }

    app.define_name_user(*t_name.as.name, val->copy());
    return val;
  }

  // internal
  unique_ptr<Val> parseElement(App& app, istream_iterator<Token>& lexer) {
    if (eos == lexer) expectedContinuation("scanning element", *lexer);

    auto val = Token::Type::DEF == lexer->type
      ? specialFunctionDef(app, lexer)
      : parseAtom(app, lexer);
    if (!val) expected("atom", *lexer);

    while (eos != lexer
        && Token::Type::THEN != lexer->type
        && Token::Type::PASS != lexer->type
        && Token::Type::LIT_LST_CLOSE != lexer->type
        && Token::Type::SUB_CLOSE != lexer->type
        && Token::Type::END != lexer->type
    ) {
      unique_ptr<Fun> base = coerse<Fun>(move(val), val->type());
      unique_ptr<Val> arg = parseAtom(app, lexer);
      if (arg) val = (*base)(move(arg));
    }

    if (Token::Type::PASS == lexer->type) {
      ++lexer;
      return parseElement(app, lexer);
    }

    return val;
  } // parseElement

  // internal
  unique_ptr<Val> parseScript(App& app, istream_iterator<Token>& lexer) {
    if (eos == lexer) expectedContinuation("scanning sub-script", *lexer);

    // early return: single value in script / sub-script
    auto val = parseElement(app, lexer);
    if (eos == lexer
      || Token::Type::SUB_CLOSE == lexer->type
      || Token::Type::END == lexer->type)
      return val;

    // early return: syntax error, to caller to handle
    if (Token::Type::THEN != lexer->type)
      return nullptr;

    // the parsed list of values was eg. "[a, b, c]";
    // this represent the composition "c . b . a" (hs
    // notations) but in the facts, here are 2 cases that
    // must be dealt with:
    // - either it is a chain of function, on par with
    //   script, so not much can be done without the last
    //   `x`: "c(b(a(x)))"
    // - or it starts with a _value_, so the whole can
    //   be computed: "c(b(a))" (only the first on can be
    //   not a function for obvious reasons)
    // the problem is the first case, because nothing can
    // actually be done about it _yet_, so the computation
    // is packed in a `FunChain` object (so that it is
    // itself a function)
    bool isfun = Ty::FUN == val->type().base();

    if (isfun) {
      // first is a function, pack all up for later
      vector<unique_ptr<Fun>> elms;
      elms.push_back(coerse<Fun>(move(val), val->type()));
      do {
        auto tmp = parseElement(app, ++lexer);
        elms.push_back(coerse<Fun>(move(tmp), tmp->type()));
      } while (eos != lexer && Token::Type::THEN == lexer->type);
      return make_unique<FunChain>(move(elms));
    }

    // first is not a function, apply all right away
    do {
      auto tmp = parseElement(app, ++lexer);
      val = (*coerse<Fun>(move(tmp), tmp->type()))(move(val));
    } while (eos != lexer && Token::Type::THEN == lexer->type);
    return val;
  } // parseScript

  unique_ptr<Val> parseApplication(App& app, istream& in) {
    istream_iterator<Token> lexer(in);
    if (eos == lexer) expectedContinuation("scanning script", *lexer);

    unique_ptr<Val> tmp = parseScript(app, lexer);
    if (Token::Type::SUB_CLOSE == lexer->type)
      throw ParseError("unmatched closing ]", lexer->loc, lexer->len);

    return tmp;
  }

} // namespace sel
