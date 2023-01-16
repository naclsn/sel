#include <unordered_map>

#define TRACE(...)
#include "sel/utils.hpp"
#include "sel/types.hpp"
#include "sel/errors.hpp"

namespace sel {

  // internal
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

  Type::Type(Ty base, Type::P p, uint8_t flags)
    : base(base)
    , p(p)
    , flags(flags)
  { TRACE(Type, raw(this)<<"; "<<base); }

  Type::Type(Type const& ty) {
    TRACE(Type&, raw(this)<<"; "<<ty);
    switch (base = ty.base) {
      case Ty::UNK:
        p.name = new std::string(*ty.p.name);
        break;

      case Ty::LST:
        p.box_has = new std::vector<Type*>();
        p.box_has->reserve(ty.p.box_has->size());
        for (auto const& it : *ty.p.box_has)
          p.box_has->push_back(new Type(*it));
        break;

      case Ty::FUN:
        p.box_pair[0] = new Type(*ty.p.box_pair[0]);
        p.box_pair[1] = new Type(*ty.p.box_pair[1]);
        break;

      default: ;
    }
    flags = ty.flags;
  }

  Type::Type(Type&& ty) noexcept {
    TRACE(Type&&, raw(this)<<"; "<<ty);
    switch (base = ty.base) {
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
    TRACE(~Type, raw(this)<<"; "<<base);
    switch (base) {
      case Ty::UNK:
        delete p.name;
        p.name = nullptr;
        break;

      case Ty::LST:
        if (p.box_has)
          for (auto const& it : *p.box_has)
            delete it;
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

  typedef std::unordered_map<std::string, Type const&> known_map;
  /**
   * now_known, has_unknowns;
   * DSF through hu for unknowns, build mapping from names to corresponding in nk;
   * both types must have the same kind ("shape")
   */
  // internal
  void recurseFindUnkowns(known_map& map, Type const& nk, Type const& hu) {
    TRACE(recurseNowKnown
      , "nk: " << nk << ", hu: " << hu
      , "in map: " << map.size() << " entries"
      );
    // assert nk.base == hu.base;

    switch (hu.base) {
      case Ty::UNK:
        map.emplace(*hu.p.name, nk);
        break;

      case Ty::NUM:
        break;

      case Ty::STR:
        // YYY: this is a (frustrating) hack, but it could be made into a real thing (named-bound'ed types)
        if ((TyFlag::IS_INF & hu.flags) && (TyFlag::IS_INF & nk.flags) != (TyFlag::IS_INF & map.at("_*").flags)) {
          map.erase("_*");
          map.emplace("_*", Type(Ty::UNK, {0}, (TyFlag::IS_INF & nk.flags)));
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
            recurseFindUnkowns(map, *nk_has[k], *hu_has[k]);

          // YYY: this is a (frustrating) hack, but it could be made into a real thing (named-bound'ed types)
          if ((TyFlag::IS_INF & hu.flags) && (TyFlag::IS_INF & nk.flags) != (TyFlag::IS_INF & map.at("_*").flags)) {
            map.erase("_*");
            map.emplace("_*", Type(Ty::UNK, {0}, (TyFlag::IS_INF & nk.flags)));
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
   * deep copy of tt, filling unkowns by picking from map
   */
  // internal
  Type recurseBuildKnown(known_map const& map, Type const& tt) {
    TRACE(recurseBuildKnown
      , "map: " << map.size() << " entries, tt: " << tt
      );

    switch (tt.base) {
      case Ty::UNK:
        {
          auto const& iter = map.find(*tt.p.name);
          return iter == map.end()
            ? Type(tt)
            : Type(iter->second);
        }

      case Ty::NUM:
        return Type(Ty::NUM, {0}, 0);

      case Ty::STR:
        return Type(Ty::STR, {0}, map.at("_*").flags);

      case Ty::LST:
        {
          auto* w = new std::vector<Type*>();
          w->reserve(tt.has().size());
          for (auto const& it : tt.has())
            w->push_back(new Type(recurseBuildKnown(map, *it)));
          return Type(Ty::LST, {.box_has=w}, map.at("_*").flags);
        }

      case Ty::FUN:
        return Type(Ty::FUN,
          {.box_pair={
            new Type(recurseBuildKnown(map, tt.from())),
            new Type(recurseBuildKnown(map, tt.to()))
          }}, 0
        );
    }

    return Type(); // unreachable
  }

  Type Type::applied(Type const& arg) const {
    // Arg and from() have same shape.
    // In from(), there are UNK, also found in to().
    // Goal is to find the corresponding types in arg and fill that in into to().
    // This is basically doing a deep-clone of to(), swapping UNKs from from() with matching in arg.

    known_map map;

    // YYY: hack to propagate bounded-ness
    map.emplace("_*", Type(Ty::UNK, {0}, TyFlag::IS_INF));

    recurseFindUnkowns(map, arg, from());

    TRACE(Type::applied
      , "arg: " << arg
      , [&map](){
          std::cerr << "map of names:";
          for (auto const& it : map)
            if ("_*" != it.first)
              std::cerr << "\n|\t   " << it.first << " <- " << it.second;
          return "";
        }()
      , "_*: " << (TyFlag::IS_INF & map.at("_*").flags ? "INF" : "FIN")
      );

    return recurseBuildKnown(map, to());
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
        expected("type expression", first);

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
          expected("type name", first);
        break;

      case TyTokenType::P_OPEN:
        if (eos == tts) expected("type expression after '('", TyToken());
        new_first = *tts;
        parseTypeImpl(new_first, ++tts, res);
        if (eos == tts) expected("token ',' or matching token ')'", TyToken());
        //        | (type, type)
        if (TyTokenType::COMMA == tts->type) {
          auto* v = new std::vector<Type*>();
          v->push_back(new Type(std::move(res)));
          res.p.box_has = v;
          do {
            new_first = *++tts;
            if (eos == tts) expected("type expression after ','", TyToken());
            Type* it = new Type();
            parseTypeImpl(new_first, ++tts, *it);
            res.p.box_has->push_back(it);
          } while (TyTokenType::COMMA == tts->type);
          res.base = Ty::LST;
          res.flags = TyFlag::IS_TPL;
        }
        if (eos == tts) expected("matching token ')'", TyToken());
        if (TyTokenType::P_CLOSE != tts->type) expected("matching token ')'", *tts);
        ++tts;
        break;

      case TyTokenType::B_OPEN:
        if (eos == tts) expected("type expression after '['", TyToken());
        res.p.box_has = new std::vector<Type*>();
        do {
          new_first = *tts;
          Type* it = new Type();
          parseTypeImpl(new_first, ++tts, *it);
          res.p.box_has->push_back(it);
          if (TyTokenType::COMMA != tts->type) break;
          ++tts;
        } while (eos != tts);
        if (eos == tts) expected("matching token ']'", TyToken());
        if (TyTokenType::B_CLOSE != tts->type) expected("matching token ']'", *tts);
        res.base = Ty::LST;
        res.flags = 0;
        ++tts;
        break;

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
      res.p.box_pair[0] = new Type(std::move(res));
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

    parseTypeImpl(first, lexer, res);
  }

  /**
   * Two types compare equal (are compatible) if any:
   * - any UNK
   * - both STR or both NUM
   * - both LST and recursive on has
   * - both FUN and recursive on from and to
   *
   * As such, this does not look at the flags (eg. IS_INF).
   */
  bool Type::operator==(Type const& other) const {
    if (Ty::UNK == base || Ty::UNK == other.base)
      return true;

    if (base != other.base)
      return false;

    // XXX: disabled for now (support too partial)
    //if ((TyFlag::IS_INF & flags) != (TyFlag::IS_INF & other.flags))
    //  return false;

    switch (base) {
      case Ty::NUM:
      case Ty::STR:
        return true;

      case Ty::LST:
        return true; // YYY: proper
        return *p.box_has == *other.p.box_has;

      case Ty::FUN:
        return *p.box_pair[0] == *other.p.box_pair[0]
            && *p.box_pair[1] == *other.p.box_pair[1];

      // unreachable
      default: return false;
    }
  }
  bool Type::operator!=(Type const& other) const {
    return !(*this == other);
  }

  std::ostream& operator<<(std::ostream& out, Type const& ty) {
    switch (ty.base) {
      case Ty::UNK:
        out << ty.p.name->substr(0, ty.p.name->find('_'));
        break;

      case Ty::NUM:
        out << "Num";
        break;

      case Ty::STR:
        out << "Str";
        break;

      case Ty::LST:
        // early break: list with no types
        if (0 == ty.p.box_has->size()) {
          out << "[_mixed]"; // ZZZ: right...
          break;
        }
        out << (TyFlag::IS_TPL & ty.flags
          ? "("
          : "[");
        out << *(*ty.p.box_has)[0];
        for (size_t k = 1, n = ty.p.box_has->size(); k < n; k++)
          out << ", " << *(*ty.p.box_has)[k];
        out << (TyFlag::IS_TPL & ty.flags
          ? ")"
          : "]");
        break;

      case Ty::FUN:
        if (Ty::FUN == ty.p.box_pair[0]->base)
          out << "(" << *ty.p.box_pair[0] << ") -> " << *ty.p.box_pair[1];
        else
          out << *ty.p.box_pair[0] << " -> " << *ty.p.box_pair[1];
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
