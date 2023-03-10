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

namespace sel {

  double NumLiteral::value() { return n; }
  ref<Val> NumLiteral::copy() const { return ref<NumLiteral>(h.app(), n); }

  std::ostream& StrLiteral::stream(std::ostream& out) { read = true; return out << s; }
  bool StrLiteral::end() { return read || s.empty(); }
  std::ostream& StrLiteral::entire(std::ostream& out) { read = true; return out << s; }
  ref<Val> StrLiteral::copy() const { return ref<StrLiteral>(h.app(), s); }

  ref<Val> LstLiteral::operator*() { return v[c]; }
  Lst& LstLiteral::operator++() { c++; return *this; }
  bool LstLiteral::end() { return v.size() <= c; }
  ref<Val> LstLiteral::copy() const {
    std::vector<ref<Val>> w;
    w.reserve(v.size());
    for (auto const& it : v)
      w.push_back(it->copy());
    return ref<LstLiteral>(h.app(), w, std::vector<Type>(ty.has()));
  }

  ref<Val> FunChain::operator()(ref<Val> arg) {
    ref<Val> r = arg;
    for (auto& it : f)
      r = (*it)(r);
    return r;
  }
  ref<Val> FunChain::copy() const {
    std::vector<ref<Fun>> g;
    g.reserve(f.size());
    for (auto const& it : f)
      g.push_back(it->copy());
    return ref<FunChain>(h.app(), g);
  }

  double NumDefine::value() { return v->value(); }
  ref<Val> NumDefine::copy() const { return ref<NumDefine>(h.app(), name, doc, v->copy()); }

  std::ostream& StrDefine::stream(std::ostream& out) { return v->stream(out); }
  bool StrDefine::end() { return v->end(); }
  std::ostream& StrDefine::entire(std::ostream& out) { return v->entire(out); }
  ref<Val> StrDefine::copy() const { return ref<StrDefine>(h.app(), name, doc, v->copy()); }

  ref<Val> LstDefine::operator*() { return v->operator*(); }
  Lst& LstDefine::operator++() { return v->operator++(); }
  bool LstDefine::end() { return v->end(); }
  ref<Val> LstDefine::copy() const { return ref<LstDefine>(h.app(), name, doc, v->copy()); }

  ref<Val> FunDefine::operator()(ref<Val> arg) { return v->operator()(arg); }
  ref<Val> FunDefine::copy() const { return ref<FunDefine>(h.app(), name, doc, v->copy()); }

  std::ostream& Input::stream(std::ostream& out) {
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
      if (std::char_traits<char>::eof() != c) {
        nowat++;
        buffer->upto++;
        out.put(c);
        buffer->cache.put(c);
      }
    } // 0 < avail
    return out;
  }
  bool Input::end() {
    return nowat == buffer->upto && (buffer->in.eof() || std::istream::traits_type::eof() == buffer->in.peek());
  }
  std::ostream& Input::entire(std::ostream& out) {
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
  ref<Val> Input::copy() const { return ref<Input>(h.app(), *this); }

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
  double parseNumber(std::istream& in, size_t* len) {
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
  std::ostream& reprToken(std::ostream& out, Token const& t) {
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
        out << ".str="; if (t.as.str) out << quoted(*t.as.str); else out << "-nil-";
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
  std::ostream& operator<<(std::ostream& out, Token const& t) {
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
        out << "literal string " << quoted(*t.as.str);
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
    std::ostringstream oss;
    oss << "expected " << should << " but got " << got << " instead";
    throw ParseError(oss.str(), got.loc, got.len);
  }
  void expectedMatching(Token const& open, Token const& got) {
    std::ostringstream oss;
    oss << "expected closing match for " << open << " but got " << got << " instead";
    throw ParseError(oss.str(), open.loc, got.loc-open.loc);
  }
  void expectedContinuation(char const* whiledotdotdot, Token const& somelasttoken) {
    std::ostringstream oss;
    oss << "reached end of script while " << whiledotdotdot;
    throw ParseError(oss.str(), somelasttoken.loc, somelasttoken.len);
  }
  void _fakeThrowErr(Token const& hl) {
    std::ostringstream oss;
    oss << "this is a fake error for testing purpose, token: " << hl;
    throw ParseError(oss.str(), hl.loc, hl.len);
  }

  // internal
  std::istream& operator>>(std::istream& in, Token& t) {
    char c = in.peek();
    if (in.eof()) {
      t.type = Token::Type::END;
      return in;
    }
    if ('#' == c) {
      in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
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
        t.as.str = new std::string();
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
          t.as.name = new std::string();
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

        throw ParseError(std::string("unexpected character '") + c + "' (" + std::to_string((int)c) + ")", t.loc, 1);
    } // switch c

    while (!in.eof() && isspace(in.peek())) in.get();
    return in;
  }

  // internal
  ref<Val> lookup(App& app, std::string const& name) {
    ref<Val> user = app.lookup_name_user(name);
    if (user) return user;
    return lookup_name(app, name);
  }
  ref<Val> lookup_unary(App& app, Token const& t) {
    switch (t.as.chr) {
      // case '@': val = static_lookup_name(app, ..); break;
      case '%': return static_lookup_name(app, flip); break; // YYY: flips cancel out
    }
    // default unreachable
    throw TypeError(std::string("unreachable in lookup_unary for character '") + t.as.chr + '\'');
  }
  ref<Val> lookup_binary(App& app, Token const& t) {
    switch (t.as.chr) {
      case '+': return static_lookup_name(app, add); break;
      case '-': return static_lookup_name(app, sub); break;
      case '.': return static_lookup_name(app, mul); break;
      case '/': return static_lookup_name(app, div); break;
      case '_': return static_lookup_name(app, index); break;
    }
    // default unreachable
    throw TypeError(std::string("unreachable in lookup_binary for character '") + t.as.chr + '\'');
  }

  // internal
  static std::istream_iterator<Token> eos;
  ref<Val> parseAtom(App& app, std::istream_iterator<Token>& lexer);
  ref<Val> parseElement(App& app, std::istream_iterator<Token>& lexer);
  ref<Val> parseScript(App& app, std::istream_iterator<Token>& lexer);

  // internal
  ref<Val> parseAtom(App& app, std::istream_iterator<Token>& lexer) {
    if (eos == lexer) expectedContinuation("scanning atom", *lexer);

    ref<Val> val(app, nullptr);
    Token t = *lexer;

    switch (t.type) {
      case Token::Type::END:
        break;

      case Token::Type::NAME:
        val = lookup(app, *t.as.name);
        if (!val) throw ParseError("unknown name '" + *t.as.name + "'", t.loc, t.len);
        ++lexer;
        break;

      case Token::Type::LIT_NUM:
        val = ref<NumLiteral>(app, t.as.num);
        ++lexer;
        break;

      case Token::Type::LIT_STR:
        val = ref<StrLiteral>(app, std::string(*t.as.str));
        ++lexer;
        break;

      case Token::Type::LIT_LST_OPEN:
        {
          std::vector<ref<Val>> elms;
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

          val = ref<LstLiteral>(app, elms);
          ++lexer;
        }
        break;

      case Token::Type::UN_OP:
        val = lookup_unary(app, t);
        {
          ref<Val> arg(app, nullptr);
          Token t = *++lexer;
          if (Token::Type::BIN_OP == t.type) {
            arg = (*static_lookup_name(app, flip))(lookup_binary(app, t));
            ++lexer;
          } else arg = parseAtom(app, lexer);
          if (!arg) expected("atom", *lexer);
          val = (*(ref<Fun>)val)(arg);
        }
        break;

      case Token::Type::BIN_OP:
        {
          // it parses the argument first, the op is on the stack for below
          ref<Val> arg = parseAtom(app, ++lexer);
          if (!arg) expected("atom", *lexer);
          // FIXME: this cast checked?
          val = (*(ref<Fun>)(*static_lookup_name(app, flip))(lookup_binary(app, t)))(arg);
        }
        break;

      case Token::Type::SUB_OPEN:
        {
          if (Token::Type::SUB_CLOSE == (++lexer)->type)
            throw ParseError("expected element, got empty sub-expression", t.loc, lexer->loc-t.loc);

          val = parseScript(app, lexer);
          if (!val) expectedMatching(t, *lexer);

          if (Token::Type::SUB_CLOSE != lexer->type) {
            if (eos == lexer) expectedContinuation("scanning sub-script element", *lexer);
            expectedMatching(t, *lexer);
          }
          ++lexer;
        }
        break;

      // YYY: (probly) unreachables
      case Token::Type::DEF:
      case Token::Type::LIT_LST_CLOSE:
      case Token::Type::SUB_CLOSE:
      case Token::Type::THEN:
      case Token::Type::PASS:
        break;
    }

    // if (!val) expected("atom", t);
    return val;
  } // parseAtom

  // internal
  ref<Val> parseElement(App& app, std::istream_iterator<Token>& lexer) {
    if (eos == lexer) expectedContinuation("scanning element", *lexer);

    ref<Val> val(app, nullptr);

    if (Token::Type::DEF == lexer->type) {
      Token t_name = *++lexer;
      if (Token::Type::NAME != t_name.type) expected("name", t_name);

      Token t_help = *++lexer;
      if (Token::Type::LIT_STR != t_help.type) expected(("docstring for '" + *t_name.as.name + "'").c_str(), t_help);

      if (lookup(app, *t_name.as.name)) {
        //throw TypeError
        throw ParseError("cannot redefine already known name '" + *t_name.as.name + "'", t_name.loc, t_name.len);
      }

      ++lexer;
      val = parseAtom(app, lexer);
      if (!val) expected("atom", *lexer);

      // trim, join, squeeze..
      std::string::size_type head = t_help.as.str->find_first_not_of("\t\n\r "), tail;
      bool blank = std::string::npos == head;
      std::string clean;
      if (!blank) {
        clean.reserve(t_help.as.str->length());
        while (std::string::npos != head) {
          tail = t_help.as.str->find_first_of("\t\n\r ", head);
          clean.append(*t_help.as.str, head, std::string::npos == tail ? tail : tail-head).push_back(' ');
          head = t_help.as.str->find_first_not_of("\t\n\r ", tail);
        }
        clean.pop_back();
        clean.shrink_to_fit();
      }

      switch (val->type().base()) {
        case Ty::NUM: val = ref<NumDefine>(app, *t_name.as.name, clean, val); break;
        case Ty::STR: val = ref<StrDefine>(app, *t_name.as.name, clean, val); break;
        case Ty::LST: val = ref<LstDefine>(app, *t_name.as.name, clean, val); break;
        case Ty::FUN: val = ref<FunDefine>(app, *t_name.as.name, clean, val); break;
        default: {
          std::ostringstream oss;
          throw TypeError((oss << "unexpected type in def expression: '" << val->type() << "'", oss.str()));
        }
      }

      app.define_name_user(*t_name.as.name, val);

    } else val = parseAtom(app, lexer);
    if (!val) expected("atom", *lexer);

    while (eos != lexer
        && Token::Type::THEN != lexer->type
        && Token::Type::PASS != lexer->type
        && Token::Type::LIT_LST_CLOSE != lexer->type
        && Token::Type::SUB_CLOSE != lexer->type
        && Token::Type::END != lexer->type
    ) {
      ref<Fun> base = coerse<Fun>(app, val, val->type());
      ref<Val> arg = parseAtom(app, lexer);
      if (arg) val = base->operator()(arg);
    }

    if (Token::Type::PASS == lexer->type) {
      ++lexer;
      return parseElement(app, lexer);
    }

    return val;
  } // parseElement

  // internal
  ref<Val> parseScript(App& app, std::istream_iterator<Token>& lexer) {
    if (eos == lexer) expectedContinuation("scanning sub-script", *lexer);

    ref<Val> val(app, nullptr);

    // early return: single value in script / sub-script
    val = parseElement(app, lexer);
    if (eos == lexer
      || Token::Type::SUB_CLOSE == lexer->type
      || Token::Type::END == lexer->type)
      return val;

    // early return: syntax error, to caller to handle
    if (Token::Type::THEN != lexer->type)
      return ref<Val>(app, nullptr);

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
      std::vector<ref<Fun>> elms;
      elms.push_back(coerse<Fun>(app, val, val->type()));
      do {
        auto tmp = parseElement(app, ++lexer);
        elms.push_back(coerse<Fun>(app, tmp, tmp->type()));
      } while (eos != lexer && Token::Type::THEN == lexer->type);
      return ref<FunChain>(app, elms);
    }

    // first is not a function, apply all right away
    do {
      auto tmp = parseElement(app, ++lexer);
      val = (*coerse<Fun>(app, tmp, tmp->type()))(val);
    } while (eos != lexer && Token::Type::THEN == lexer->type);
    return val;
  } // parseScript

  ref<Val> parseApplication(App& app, std::istream& in) {
    std::istream_iterator<Token> lexer(in);
    if (eos == lexer) expectedContinuation("scanning script", *lexer);

    ref<Val> tmp = parseScript(app, lexer);
    if (Token::Type::SUB_CLOSE == lexer->type)
      throw ParseError("unmatched closing ]", lexer->loc, lexer->len);

    return tmp;
  }

} // namespace sel
