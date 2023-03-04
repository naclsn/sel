#include "sel/errors.hpp"
#include "sel/types.hpp"
#include "sel/utils.hpp"

namespace sel {

  std::ostream& operator<<(std::ostream& out, Ty ty) {
    switch (ty) {
      case Ty::UNK: out << "UNK"; break;
      case Ty::NUM: out << "NUM"; break;
      case Ty::STR: out << "STR"; break;
      case Ty::LST: out << "LST"; break;
      case Ty::FUN: out << "FUN"; break;
    }
    return out;
  }

  // internal
  std::ostream& operator<<(std::ostream& out, TyFlag tf) {
    out << (TyFlag::IS_INF & tf ? "IS_INF" : "IS_FIN");
    return out;
  }

  Type::Type(Type const& ty) {
    switch (_base = ty.base()) {
      case Ty::UNK:
        p.name = new std::string(ty.name());
        break;

      case Ty::LST:
        p.box_has = new std::vector<Type>(ty.has());
        break;

      case Ty::FUN:
        p.box_pair[0] = new Type(ty.from());
        p.box_pair[1] = new Type(ty.to());
        break;

      default: ;
    }
    flags = ty.flags;
  }

  Type::Type(Type&& ty) noexcept {
    switch (_base = ty._base) {
      case Ty::UNK:
        p.name = ty.p.name;
        ty.p.name = nullptr;
        break;

      case Ty::LST:
        p.box_has = ty.p.box_has;
        ty.p.box_has = nullptr;
        break;

      case Ty::FUN:
        p.box_pair[0] = ty.p.box_pair[0];
        p.box_pair[1] = ty.p.box_pair[1];
        ty.p.box_pair[0] = nullptr;
        ty.p.box_pair[1] = nullptr;
        break;

      default: ;
    }
    flags = ty.flags;
  }

  Type::~Type() {
    switch (_base) {
      case Ty::UNK:
        delete p.name;
        p.name = nullptr;
        break;

      case Ty::LST:
        delete p.box_has;
        p.box_has = nullptr;
        break;

      case Ty::FUN:
        delete p.box_pair[0];
        delete p.box_pair[1];
        p.box_pair[0] = nullptr;
        p.box_pair[1] = nullptr;
        break;

      default: ;
    }
  }

  Type& Type::operator=(Type ty) {
    switch (_base = ty.base()) {
      case Ty::UNK:
        std::swap(p.name, ty.p.name);
        break;

      case Ty::LST:
        std::swap(p.box_has, ty.p.box_has);
        break;

      case Ty::FUN:
        std::swap(p.box_pair, ty.p.box_pair);
        break;

      default: ;
    }
    flags = ty.flags;
    return *this;
  }

  void Type::repr(std::ostream& out, unsigned depth) const {
    char indent[depth*3+1];
    for (size_t k = 0; k < depth*3; k++)
      indent[k] = ' ';
    indent[depth*3] = '\0';

    out << "Type {\n" << indent << "base= ";

    switch (_base) {
      case Ty::UNK:
        out << "UNK";
        out << "\n" << indent << "name= " << quoted(name());
        break;

      case Ty::NUM:
        out << "NUM";
        break;

      case Ty::STR:
        out << "STR";
        out << "\n" << indent << "is_inf= " << std::boolalpha << !!(flags & TyFlag::IS_INF);
        break;

      case Ty::LST:
        out << "LST";
        out << "\n" << indent << "is_inf= " << std::boolalpha << !!(flags & TyFlag::IS_INF);
        out << "\n" << indent << "is_tpl= " << std::boolalpha << !!(flags & TyFlag::IS_TPL);
        out << "\n" << indent << "has= {\n";
        for (const auto& it : has())
          it.repr(out << indent << indent, depth+2);
        out << indent << "}";
        break;

      case Ty::FUN:
        out << "FUN";
        from().repr(out << "\n" << indent << "from= ", depth+1);
        to().repr(out << "\n" << indent << "to= ", depth+1);
        break;
    }

    if (depth) indent[(depth-1)*3] = '\0';
    out << "\n" << indent << "}\n";
  }

  /**
   * now_known, has_unknowns;
   * DSF through hu for unknowns, build mapping from names to corresponding in nk;
   * both types must have the same kind ("shape")
   */
  // internal - friend
  void recurseFindUnkowns(Type::known_map& map, Type const& nk, Type const& hu) {
    switch (hu.base()) {
      case Ty::UNK:
        map.emplace(hu.name(), nk);
        break;

      case Ty::NUM:
        break;

      case Ty::STR:
        // YYY: this is a (garbage) hack, but it could be made into a real thing (named-bound'ed types)
        if ((TyFlag::IS_INF & hu.flags) && (TyFlag::IS_INF & nk.flags) != (TyFlag::IS_INF & map.at("_*").flags)) {
          map.erase("_*");
          Type ty;
          ty.flags = TyFlag::IS_INF & nk.flags;
          map.emplace("_*", std::move(ty));
        }
        break;

      case Ty::LST:
        // assert nk.has().size() == hu.has().size()
        // FIXME: `const {}, reverse, join: :` (MRE)
        // because the parsing does not give its 'has'
        // to a literal list, it gets initialized empty
        // the problem is likely not to be solved here
        // but in type coersion, where it will be done
        // to make the input type compatible; eg:
        // '[Str]' from '(Num, Str)' is valid and will
        // be applied the missing conversion(s)
        // by `coerse()` so that the output val
        // effectively always have a type compatible
        // to the function it gets applied to
        {
          auto const& hu_has = hu.has();
          auto const& nk_has = nk.has();
          for (size_t k = 0; k < hu_has.size(); k++)
            recurseFindUnkowns(map, nk_has[k], hu_has[k]);

          // YYY: this is a (garbage) hack, but it could be made into a real thing (named-bound'ed types)
          if ((TyFlag::IS_INF & hu.flags) && (TyFlag::IS_INF & nk.flags) != (TyFlag::IS_INF & map.at("_*").flags)) {
            map.erase("_*");
            Type ty;
            ty.flags = TyFlag::IS_INF & nk.flags;
            map.emplace("_*", std::move(ty));
          }
        }
        break;

      case Ty::FUN:
        recurseFindUnkowns(map, nk.from(), hu.from());
        recurseFindUnkowns(map, nk.to(), hu.to());
        break;
    }

    // unreachable
  }

  /**
   * template_type
   * deep copy of tt into ty, filling unkowns by picking from map
   */
  // internal - friend
  void recurseBuildKnown(Type::known_map const& map, Type const& tt, Type& ty) {
    switch (tt.base()) {
      case Ty::UNK: {
          auto const& iter = map.find(tt.name());
          ty = iter == map.end() ? tt : iter->second;
        } break;

      case Ty::NUM:
        ty._base = Ty::NUM;
        ty.flags = 0;
        break;

      case Ty::STR:
        ty._base = Ty::STR;
        ty.flags = map.at("_*").flags;
        break;

      case Ty::LST: {
          auto& w = ty.makeHas();
          w.reserve(tt.has().size());
          for (auto const& it : tt.has()) {
            w.emplace_back();
            recurseBuildKnown(map, it, w.back());
          }

          ty._base = Ty::LST;
          ty.flags = tt.flags & TyFlag::IS_TPL ? TyFlag::IS_TPL : map.at("_*").flags;
        } break;

      case Ty::FUN: {
          ty._base = Ty::FUN;
          ty.flags = 0;
          recurseBuildKnown(map, tt.from(), ty.makeFrom());
          recurseBuildKnown(map, tt.to(), ty.makeTo());
        } break;
    }
  }

  void Type::applied(Type const& arg, Type& res) const {
    // Arg and from() have same shape.
    // In from(), there are UNK, also found in to().
    // Goal is to find the corresponding types in arg and fill that in into to().
    // This is basically doing a deep-clone of to(), swapping UNKs from from() with matching in arg.

    known_map map;

    // YYY: hack to propagate bounded-ness
    {
      Type ty;
      ty.flags = TyFlag::IS_INF;
      map.emplace("_*", ty);
    }

    recurseFindUnkowns(map, arg, from());
    recurseBuildKnown(map, to(), res);
  }

  // internal
  enum class TyTokenType {
    END,
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
      case TyTokenType::END:     out << "END";     break;
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
  void expected(char const* should, TyToken const& got) { // + tts->text + "..."
    std::ostringstream oss;
    oss << "expected " << should << " but got " << got << " instead";
    throw ParseError(oss.str(), 0, 1);
    // YYY: TyTokens do not store any location information...
    // but for not this is ok as there's never a need to parse a type
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

      default:
        if (isspace(c)) break;
        if ((c < 'A' || 'Z' < c) && (c < 'a' || 'z' < c)) goto unknown_token_push1;

        tt.type = islower(c)
          ? TyTokenType::NAME
          : TyTokenType::TY_NAME;
        tt.text = c;
        while ('a' <= in.peek() && in.peek() <= 'z') // no upper cases? should not, only first character
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

  // internal - friend
  void parseTypeImpl(TyToken&& first, std::istream_iterator<TyToken>& tts, Type& res) {
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
        expected("type expression", first);

      case TyTokenType::NAME:
        res._base = Ty::UNK;
        res.makeName() = std::move(first.text);
        break;

      case TyTokenType::TY_NAME:
        if      ("Num" == first.text) res._base = Ty::NUM;
        else if ("Str" == first.text) res._base = Ty::STR;
        else expected("type name", first);
        break;

      case TyTokenType::P_OPEN:
        if (eos == tts) expected("type expression after '('", TyToken());

        new_first = *tts;
        parseTypeImpl(std::move(new_first), ++tts, res);
        if (eos == tts) expected("token ',' or matching token ')'", TyToken());

        //        | (type, type)
        if (TyTokenType::COMMA == tts->type) {
          std::vector<Type> v;
          v.push_back(std::move(res));

          do {
            new_first = *++tts;
            if (eos == tts) expected("type expression after ','", TyToken());

            v.emplace_back();
            parseTypeImpl(std::move(new_first), ++tts, v.back());
          } while (TyTokenType::COMMA == tts->type);

          res._base = Ty::LST;
          res.flags = TyFlag::IS_TPL;
          res.makeHas() = std::move(v);
        }

        if (eos == tts) expected("matching token ')'", TyToken());
        if (TyTokenType::P_CLOSE != tts->type) expected("matching token ')'", *tts);

        ++tts;
        break;

      case TyTokenType::B_OPEN: {
        if (eos == tts) expected("type expression after '['", TyToken());

        auto& v = res.makeHas();

        do {
          new_first = *tts;

          v.emplace_back();
          parseTypeImpl(std::move(new_first), ++tts, v.back());

          if (TyTokenType::COMMA != tts->type) break;
          ++tts;
        } while (eos != tts);

        if (eos == tts) expected("matching token ']'", TyToken());
        if (TyTokenType::B_CLOSE != tts->type) expected("matching token ']'", *tts);

        res._base = Ty::LST;
        res.flags = 0;

        ++tts;
        } break;

      default:
        expected("type expression", first);
    }

    if (eos == tts) return;
    //        | type*
    if (TyTokenType::STAR == tts->type) {
      res.flags|= TyFlag::IS_INF;
      ++tts;
    }

    if (eos == tts) return;
    //        | type -> type
    if (TyTokenType::ARROW == tts->type) {
      Type backup(std::move(res));
      res.makeFrom() = std::move(backup);

      new_first = *++tts;
      parseTypeImpl(std::move(new_first), ++tts, res.makeTo());

      res._base = Ty::FUN;
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
        ++lexer; // drop '::'
        if (eos == lexer) throw std::string("end of token stream");
        if (named) named->assign(first.text);
        first = *lexer; // pop `first` (will forward to impl)
        ++lexer;
      } else if (named) named->assign("");
    } else {
      if (named) named->assign("");
      ++lexer; // pop `first` (will forward to impl)
    }

    parseTypeImpl(std::move(first), lexer, res);
  }

  // internal
  bool recurseEqual(Type::known_map& map, Type const& a, Type const& b) {
    if (a.base() != b.base()) {
      if (Ty::UNK == a.base()) {
        auto const& it = map.find(a.name());
        if (map.end() != it) return recurseEqual(map, it->second, b);
        map.insert({a.name(), b});
        return true; // this lets eg. '[a] == a' be true :-(
      }

      if (Ty::UNK == b.base()) {
        auto const& it = map.find(b.name());
        if (map.end() != it) return recurseEqual(map, a, it->second);
        map.insert({b.name(), a});
        return true; // this lets eg. '[a] == a' be true :-(
      }

      // none is UNK; trivial not equal
      return false;
    }

    // XXX: disabled for now (support too partial)
    //if ((TyFlag::IS_INF & a.flags) != (TyFlag::IS_INF & b.flags))
    //  return false;

    switch (a.base()) {
      case Ty::UNK:
        if (a.name() == b.name()) return true;
        // else, if one is already associated
        {
          auto const& it = map.find(a.name());
          if (map.end() != it) return recurseEqual(map, it->second, b);
        }
        {
          auto const& it = map.find(b.name());
          if (map.end() != it) return recurseEqual(map, a, it->second);
        }
        // comparing eg. 'a' and 'b'
        map.insert({a.name(), b});
        return true;

      case Ty::NUM:
      case Ty::STR:
        return true;

      case Ty::LST:
        {
          auto const len = a.has().size();
          if (len != b.has().size()) return false;
          for (size_t k = 0; k < len; k++) {
            Type const& tya = a.has()[k];
            Type const& tyb = b.has()[k];
            if (!recurseEqual(map, tya, tyb)) return false;
          }
          return true;
        }

      case Ty::FUN:
        return recurseEqual(map, a.from(), b.from())
            && recurseEqual(map, a.to(), b.to());
    }

    return false;
  }

  bool Type::operator==(Type const& other) const {
    std::unordered_map<std::string, Type const&> map;
    return recurseEqual(map, *this, other);
  }
  bool Type::operator!=(Type const& other) const {
    return !(*this == other);
  }

  std::ostream& operator<<(std::ostream& out, Type const& ty) {
    switch (ty.base()) {
      case Ty::UNK:
        out << ty.name().substr(0, ty.name().find('_'));
        break;

      case Ty::NUM:
        out << "Num";
        break;

      case Ty::STR:
        out << "Str";
        break;

      case Ty::LST:
        // early break: list with no types
        if (0 == ty.has().size()) {
          out << "[_mixed]"; // ZZZ: right...
          break;
        }
        out << (TyFlag::IS_TPL & ty.flags
          ? "("
          : "[");
        out << ty.has()[0];
        for (size_t k = 1, n = ty.has().size(); k < n; k++)
          out << ", " << ty.has()[k];
        out << (TyFlag::IS_TPL & ty.flags
          ? ")"
          : "]");
        break;

      case Ty::FUN:
        if (Ty::FUN == ty.from().base())
          out << "(" << ty.from() << ") -> " << ty.to();
        else
          out << ty.from() << " -> " << ty.to();
        break;
    }

    if (TyFlag::IS_INF & ty.flags) out << "*";
    return out;
  }

  std::istream& operator>>(std::istream& in, Type& res) {
    parseType(in, nullptr, res);
    return in;
  }

} // namespace sel
