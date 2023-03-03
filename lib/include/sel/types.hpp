#ifndef SEL_TYPE_HPP
#define SEL_TYPE_HPP

/**
 * Parsing and representation of types, as well as a set
 * of helper functions to deal with the type system.
 */

#include <istream>
#include <iterator>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace sel {

  enum class Ty {
    UNK,
    NUM,
    STR,
    LST,
    FUN,
  };
  enum TyFlag {
    IS_FIN = 0,
    IS_INF = 1,
    IS_TPL = 2,
  };

  struct TyToken;

  struct Type {
  private:
    Ty _base = Ty::UNK;
    union P {
      std::string* name;
      std::vector<Type>* box_has; // Ty has;
      Type* box_pair[2]; // Ty pair[2];
    } p = {.name=nullptr};
    uint8_t flags = 0;

  public:
    Type() { }
    Type(Type const& ty);
    Type(Type&& ty) noexcept;
    ~Type();

    Type& operator=(Type const& ty);

    void repr(std::ostream& out, unsigned indent=1) const;

    bool operator==(Type const& other) const;
    bool operator!=(Type const& other) const;

    Ty const& base() const { return _base; }

    std::string const& name() const { return *p.name; }
    void name(std::string&& set) { p.name = new std::string(std::move(set)); }

    std::vector<Type> const& has() const { return *p.box_has; }
    void has(std::vector<Type>&& set) { p.box_has = new std::vector<Type>(std::move(set)); }
    bool is_inf() const { return TyFlag::IS_INF & flags; }
    bool is_tpl() const { return TyFlag::IS_TPL & flags; }

    Type const& from() const { return *p.box_pair[0]; }
    void from(Type&& set) { p.box_pair[0] = new Type(std::move(set)); }
    Type const& to() const { return *p.box_pair[1]; }
    void to(Type&& set) { p.box_pair[1] = new Type(std::move(set)); }

    void applied(Type const& arg, Type& res) const;
    unsigned arity() const { return Ty::FUN == _base ? to().arity() + 1 : 0; }

  // static:
    inline static Type makeUnk(std::string&& name) {
      Type r;
      r._base = Ty::UNK;
      r.name(std::move(name));
      return r;
    }
    inline static Type makeNum() {
      Type r;
      r._base = Ty::NUM;
      return r;
    }
    inline static Type makeStr(bool is_inf) {
      Type r;
      r._base = Ty::STR;
      r.flags = is_inf ? TyFlag::IS_INF : TyFlag::IS_FIN;
      return r;
    }
    inline static Type makeLst(std::vector<Type>&& has, bool is_inf, bool is_tpl) {
      Type r;
      r._base = Ty::LST;
      r.flags = is_inf ? TyFlag::IS_INF : TyFlag::IS_FIN;
      if (is_tpl) r.flags|= TyFlag::IS_TPL;
      r.has(std::move(has));
      return r;
    }
    inline static Type makeFun(Type&& from, Type&& to) {
      Type r;
      r._base = Ty::FUN;
      r.from(std::move(from));
      r.to(std::move(to));
      return r;
    }

  // friend:
  // more or less temporary (idk) ((this 'private' just as rem))
  private:
    typedef std::unordered_map<std::string, Type const&> known_map;
    friend void recurseFindUnkowns(known_map& map, Type const& nk, Type const& hu);
    friend void recurseBuildKnown(known_map const& map, Type const& tt, Type& ty);

    friend bool recurseEqual(Type::known_map& map, Type const& a, Type const& b);

    friend void parseTypeImpl(TyToken&& first, std::istream_iterator<TyToken>& tts, Type& res);

  public:
    /**
     * Parse a type from the given stream. The overload to
     * the `>>` operator is also provided as a convenience,
     * but this function allows retreiving a name
     * (which would be specified with the `<name> ::`
     * syntax). If `named` is not nullptr but the parsed
     * type representation does not specify a name, the
     * empty string is written into `named`.
    */
    friend void parseType(std::istream& in, std::string* named, Type& res);

    friend std::ostream& operator<<(std::ostream& out, Type const& ty);
    friend std::istream& operator>>(std::istream& in, Type& res);
  };

  std::ostream& operator<<(std::ostream& out, Ty ty);

} // namespace sel

#endif // SEL_TYPE_HPP
