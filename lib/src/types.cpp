#include <iostream> // ZZZ: rem when no used
#include <sstream>
#include <iterator>

#include "types.hpp"

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
    if (in.eof()) return in;
    switch (c) {
      case ':':
        if (':' != in.get()) {
          tt.type = TyTokenType::UNKNOWN;
          in.unget();
          in.unget();
          return in >> tt.text;
        }
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
        if ('>' != in.get()) {
          tt.type = TyTokenType::UNKNOWN;
          in.unget();
          in.unget();
          return in >> tt.text;
        }
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
        if (!isalpha(c)) {
          tt.type = TyTokenType::UNKNOWN;
          in.unget();
          return in >> tt.text;
        }

        tt.type = islower(c)
          ? TyTokenType::NAME
          : TyTokenType::TY_NAME;
        tt.text = c;
        while (isalpha(in.peek()))
          tt.text.push_back(in.get());
    }

    while (isspace(in.peek())) in.get();
    return in;
  }

  // internal -- simple recursive parser; probly not the best, but not a requirement
  void parseTypeImpl(std::istream_iterator<TyToken>& tts, Type& res) {
    // flag indicate what was found, indicating the state (eg. cannot have both bracket and comma)
    bool parenthesis = false, bracket = false, comma = false;

    std::cerr << "first TyToken at this level: " << *tts << std::endl;

    // break and return when any of:
    // - end of iterator
    // - closing p, level above will figure the function/couple out
    // - closing b, level above will figure the list out
    // - comma, level above will figure the couple out

    res.base = Ty::UNK;
    res.p.name = new std::string("idk");
  }

  void parseType(std::istream& in, std::string* named, Type& res) {
    auto lexer = std::istream_iterator<TyToken>(in);

    // first check for "<name> '::'"
    TyToken first = *lexer;
    if (TyTokenType::NAME == first.type) {
      TyToken second = *std::next(lexer);
      if (TyTokenType::TY_EQ == second.type) {
        // if caller wants it
        if (named) named->assign(first.text);
        // skip over both
        lexer++++;
      } else named->assign("");
    } else named->assign("");

    parseTypeImpl(lexer, res);
  }

  /**
   * Two types compare equal (are compatible) if any:
   * - any UNK
   * - both STR or both NUM
   * - both LST and recursive on has
   * - both FUN and recursive on fst and snd
   * - both CPL and recursive on fst and snd
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
