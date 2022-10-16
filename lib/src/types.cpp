#include <iostream> // ZZZ: rem when no used
#include <sstream>
#include <iterator>

#include "types.hpp"
#include "errors.hpp"

namespace sel {

  Type::~Type() {
    switch (base) {
      case Ty::UNK:
        delete p.name;
        break;

      case Ty::LST:
        delete p.box_has;
        break;

      case Ty::FUN:
      case Ty::CPL:
        delete p.box_pair[0];
        delete p.box_pair[1];
        break;

      default: ;
    }
  }

  Type unkType(std::string* name) {
    return Type {
      .base=Ty::UNK,
      .p={.name=name},
    };
  }

  Type numType() {
    return Type {
      .base=Ty::NUM,
    };
  }

  Type strType(TyFlag is_inf) {
    return Type {
      .base=Ty::STR,
      .flags=static_cast<uint8_t>(is_inf),
    };
  }

  // Type lstType(Ty has, TyFlag is_inf) {
  //   return Type {
  //     .base=Ty::LST,
  //     .p={.has=has},
  //     .flags=static_cast<uint8_t>(is_inf),
  //   };
  // }
  Type lstType(Type* has, TyFlag is_inf) {
    return Type {
      .base=Ty::LST,
      .p={.box_has=has},
      .flags=static_cast<uint8_t>(is_inf),
    };
  }

  // Type funType(Ty fst, Ty snd) {
  //   return Type {
  //     .base=Ty::FUN,
  //     .p={.pair={fst,snd}},
  //   };
  // }
  Type funType(Type* fst, Type* snd) {
    return Type {
      .base=Ty::FUN,
      .p={.box_pair={fst,snd}},
    };
  }

  // Type cplType(Ty fst, Ty snd) {
  //   return Type {
  //     .base=Ty::CPL,
  //     .p={.pair={fst,snd}},
  //   };
  // }
  Type cplType(Type* fst, Type* snd) {
    return Type {
      .base=Ty::CPL,
      .p={.box_pair={fst,snd}},
      // .flags=, // YYY: don't know if it should bubble the IS_INF flag up
    };
  }

  // internal
  enum class TyTokenType {
    UNKNOWN,
    TY_EQ,
    P_OPEN, P_CLOSE,
    B_OPEN, B_CLOSE,
    ARROW,
    COMMA,
    STAR,
    NAME, // unknown named type (first letter lower case)
    TY_NAME, // known type (first letter upper case)
  };
  // internal
  struct TyToken {
    TyTokenType type;
    std::string text;
  };

  // internal
  std::ostream& operator<<(std::ostream& out, TyToken const& tt) {
    out << "TyToken { .type=";
    switch (tt.type) {
      case TyTokenType::UNKNOWN: out << "UNKNOWN"; break;
      case TyTokenType::TY_EQ:   out << "TY_EQ";   break;
      case TyTokenType::P_OPEN:  out << "P_OPEN";  break;
      case TyTokenType::P_CLOSE: out << "P_CLOSE"; break;
      case TyTokenType::B_OPEN:  out << "B_OPEN";  break;
      case TyTokenType::B_CLOSE: out << "B_CLOSE"; break;
      case TyTokenType::ARROW:   out << "ARROW";   break;
      case TyTokenType::COMMA:   out << "COMMA";   break;
      case TyTokenType::STAR:    out << "STAR";    break;
      case TyTokenType::NAME:    out << "NAME";    break;
      case TyTokenType::TY_NAME: out << "TY_NAME"; break;
    }
    return out << ", .text=\"" << tt.text << "\" }";
  }

  // internal
  std::istream& operator>>(std::istream& in, TyToken& tt) {
    char c = in.get();
    char cc = 0;
    if (in.eof()) return in;
    switch (c) {
      case ':':
        if (':' != (cc = in.get())) goto unknown_token_push2;
        tt.type = TyTokenType::TY_EQ;
        tt.text = "::";
        break;

      case '(':
        tt.type = TyTokenType::P_OPEN;
        tt.text = "(";
        break;

      case ')':
        tt.type = TyTokenType::P_CLOSE;
        tt.text = ")";
        break;

      case '[':
        tt.type = TyTokenType::B_OPEN;
        tt.text = "[";
        break;

      case ']':
        tt.type = TyTokenType::B_CLOSE;
        tt.text = "]";
        break;

      case '-':
        if ('>' != (cc = in.get())) goto unknown_token_push2;
        tt.type = TyTokenType::ARROW;
        tt.text = "->";
        break;

      case ',':
        tt.type = TyTokenType::COMMA;
        tt.text = ",";
        break;

      case '*':
        tt.type = TyTokenType::STAR;
        tt.text = "*";
        break;

      default: // TODO: `isalpha`: restrain to [A-Za-z]
        if (isspace(c)) break;
        if (!isalpha(c)) goto unknown_token_push1;

        tt.type = islower(c)
          ? TyTokenType::NAME
          : TyTokenType::TY_NAME;
        tt.text = c;
        while (isalpha(in.peek()))
          tt.text.push_back(in.get());
    }

    if (false) {
unknown_token_push2:
      tt.text = cc;
unknown_token_push1:
      if (cc) tt.text+= c;
      else    tt.text = c;
      tt.type = TyTokenType::UNKNOWN;
    }

    while (!in.eof() && isspace(in.peek())) in.get();
    return in;
  }

  // internal
  void parseTypeImpl(TyToken const& first, std::istream_iterator<TyToken>& tts, Type& res) {
    static auto const eos = std::istream_iterator<TyToken>();
    TyToken new_first;

    // type ::= name
    //        | ty_name
    //        | (type)
    //        | [type]
    //        | type -> type
    //        | (type, type)
    //        | type*
    switch (first.type) {
      case TyTokenType::UNKNOWN:
        throw ParseError("type expression",
          ( std::string("got unknown tokens")
          + " '" + first.text + " " + tts->text + " ...'"
          ), "- what -");

      case TyTokenType::NAME:
        res.base = Ty::UNK;
        res.p.name = new std::string(first.text);
        break;

      case TyTokenType::TY_NAME:
        if ("Num" == first.text)
          res.base = Ty::NUM;
        else if ("Str" == first.text)
          res.base = Ty::STR;
        else
          throw NameError(first.text, "- what -");
        break;

      case TyTokenType::P_OPEN:
        if (eos == tts) throw EOSError("type expression after '('", "- what -");
        new_first = *tts;
        parseTypeImpl(new_first, ++tts, res);
        if (eos == tts) throw EOSError("token ',' or matching token ')'", "- what -");
        //        | (type, type)
        if (TyTokenType::COMMA == tts->type) {
          res.p.box_pair[0] = new Type(res);
          new_first = *++tts;
          parseTypeImpl(new_first, ++tts, *(res.p.box_pair[1] = new Type()));
          res.base = Ty::CPL;
          res.flags = 0;
        }
        if (eos == tts) throw EOSError("matching token ')'", "- what -");
        if (TyTokenType::P_CLOSE != tts->type)
          throw ParseError("matching token ')'",
            ( std::string("got unexpected tokens")
            + " '" + tts++->text + " " + tts->text + " ...'"
            ), "- what -");
        tts++;
        break;

      case TyTokenType::B_OPEN:
        if (eos == tts) throw EOSError("type expression after '['", "- what -");
        res.p.box_has = new Type();
        new_first = *tts;
        parseTypeImpl(new_first, ++tts, *res.p.box_has);
        if (eos == tts) throw EOSError("matching token ']'", "- what -");
        if (TyTokenType::B_CLOSE != tts->type)
          throw ParseError("matching token ')'",
            ( std::string("got unexpected tokens")
            + " '" + tts++->text + " " + tts->text + " ...'"
            ), "- what -");
        res.base = Ty::LST;
        res.flags = 0;
        tts++;
        break;

      default:
        throw ParseError("type expression",
          ( std::string("got unexpected tokens")
          + " '" + first.text + " " + tts->text + " ...'"
          ), "- what -");
    }

    if (eos == tts) return;
    //        | type*
    if (TyTokenType::STAR == tts->type) {
      res.flags|= TyFlag::IS_INF;
      tts++;
    }

    if (eos == tts) return;
    //        | type -> type
    if (TyTokenType::ARROW == tts->type) {
      res.p.box_pair[0] = new Type(res);
      new_first = *++tts;
      parseTypeImpl(new_first, ++tts, *(res.p.box_pair[1] = new Type()));
      res.base = Ty::FUN;
      res.flags = 0;
    }
  }

  void parseType(std::istream& in, std::string* named, Type& res) {
    static auto const eos = std::istream_iterator<TyToken>();
    auto lexer = std::istream_iterator<TyToken>(in);

    // first check for "<name> '::'"
    // note that the whole block needs to check the 2
    // first tokens, and as such need to pop at least one;
    // `first` is handed over to the `parseTypeImple`
    // to compensate
    TyToken first = *lexer;
    if (TyTokenType::NAME == first.type && eos != lexer) {
      TyToken second = *++lexer;
      if (TyTokenType::TY_EQ == second.type) {
        lexer++; // drop '::'
        if (eos == lexer) throw std::string("end of token stream");
        if (named) named->assign(first.text);
        first = *lexer++; // pop `first` (will forward to impl)
      } else if (named) named->assign("");
    } else {
      if (named) named->assign("");
      lexer++; // pop `first` (will forward to impl)
    }

    parseTypeImpl(first, lexer, res);
  }

  /**
   * Two types compare equal (are compatible) if any:
   * - any UNK
   * - both STR or both NUM
   * - both LST and recursive on has
   * - both FUN and recursive on fst and snd
   * - both CPL and recursive on fst and snd
   *
   * As such, this does not look at the flags (eg. IS_INF).
   */
  bool Type::operator==(Type const& other) const {
    if (Ty::UNK == base || Ty::UNK == other.base)
      return true;

    if (base != other.base)
      return false;

    if ((TyFlag::IS_INF & flags) != (TyFlag::IS_INF & other.flags))
      return false;

    switch (base) {
      case Ty::NUM:
      case Ty::STR:
        return true;

      case Ty::LST:
        return *p.box_has == *other.p.box_has;

      case Ty::FUN:
      case Ty::CPL:
        return *p.box_pair[0] == *other.p.box_pair[0]
            && *p.box_pair[1] == *other.p.box_pair[1];

      // unreachable
      default: return false;
    }
  }

  std::ostream& operator<<(std::ostream& out, Type const& ty) {
    switch (ty.base) {
      case Ty::UNK:
        out << *ty.p.name;
        break;

      case Ty::NUM:
        out << "Num";
        break;

      case Ty::STR:
        out << "Str";
        break;

      case Ty::LST:
        // if (TyFlag::IS_BOX & flags)
          out << "[" << *ty.p.box_has << "]";
        // else
          // out << "[" << (Ty::NUM == p.has ? "Num" : "Str") << "]";
        break;

      case Ty::FUN:
        // if (TyFlag::IS_BOX & flags) {
          if (Ty::FUN == ty.p.box_pair[0]->base)
            out << "(" << *ty.p.box_pair[0] << ") -> " << *ty.p.box_pair[1];
          else
            out << *ty.p.box_pair[0] << " -> " << *ty.p.box_pair[1];
        // }
        // else
        //   out
        //     << (Ty::NUM == p.pair[0] ? "Num" : "Str")
        //     << " -> " << (Ty::NUM == p.pair[1] ? "Num" : "Str");
        break;

      case Ty::CPL:
        // if (TyFlag::IS_BOX & flags)
          out << "(" << *ty.p.box_pair[0] << ", " << *ty.p.box_pair[1] << ")";
        // else
        //   out
        //     << "(" << (Ty::NUM == p.pair[0] ? "Num" : "Str")
        //     << ", " << (Ty::NUM == p.pair[1] ? "Num" : "Str")
        //     << ")";
        break;
    }

    if (TyFlag::IS_INF & ty.flags) out << "*";
    return out;
  }

  std::istream& operator>>(std::istream& in, Type& tt) {
    parseType(in, nullptr, tt);
    return in;
  }

} // namespace sel
