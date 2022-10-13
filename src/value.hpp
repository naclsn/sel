#ifndef SEL_VALUE_HPP
#define SEL_VALUE_HPP

#include <iostream>
#include <string>
#include <sstream>
#include <assert.h>

#include "utils.hpp"

namespace sel {

  enum class BasicType {
    UNK, // str
    NUM, // 0
    STR, // 0
    LST, // 1
    FUN, // 2
    CPL, // 2
  };

  struct Type {
    BasicType base;
    union {
      std::string* name;
      Type* pair[2];
    } pars;

    Type(BasicType ty) {
      assert(BasicType::NUM == ty || BasicType::STR == ty);
      base = ty;
    }
    Type(BasicType ty, std::string* name) {
      assert(BasicType::UNK == ty);
      base = ty;
      pars.name = name;
    }
    Type(BasicType ty, Type* has) {
      assert(BasicType::LST == ty);
      base = ty;
      pars.pair[0] = has;
    }
    Type(BasicType ty, Type* fst, Type* snd) {
      assert(BasicType::FUN == ty || BasicType::CPL == ty);
      base = ty;
      pars.pair[0] = fst;
      pars.pair[1] = snd;
    }

    Type(std::istream& in) { }

    ~Type() {
      switch (base) {
        case BasicType::UNK:
          delete pars.name;
          break;

        case BasicType::LST:
          delete pars.pair[0];
          break;

        case BasicType::FUN:
        case BasicType::CPL:
          delete pars.pair[0];
          delete pars.pair[1];
          break;

        default: ;
      }
    }

    std::ostream& output(std::ostream& out) const;
    bool operator==(Type const& other) const;
  }; // struct Type
  std::ostream& operator<<(std::ostream& out, Type const& ty);


  // k, this was a fun attempt and works as a placeholder,
  // now TODO: make it proper
  class TypeError : public std::runtime_error {
  private:
    Type from, to;
    mutable std::string* r;
    char const* msg;

  public:
    TypeError(Type from, Type to, char const* msg): std::runtime_error("type error"), from(from), to(to), r(nullptr), msg(msg) { }
    ~TypeError() { delete r; }

    char const* what() const throw() {
      if (nullptr == r) {
        std::stringstream s;
        s << std::runtime_error::what() << ": " << msg << std::endl
          << "\tfrom: " << from << std::endl
          << "\t  to: " <<   to << std::endl
        ;
        r = new std::string(s.str());
      }
      return r->c_str();
    }

  };


  class Num;
  class Str;
  class Lst;
  class Fun;
  class Cpl;


  /**
   * The as[..] collection of methods is only implemented
   * by the corresponding type. It returns a pointer
   * to the same object or nullptr if the true type
   * of the value does not match. This is a very cheap
   * operation.
   * On the other hand, the to[..] methods will actively
   * try to coerse a value which true type does not
   * match. This always incure the creation of a new
   * object, even if the type actually matched. If the
   * coersion is not possible, nullptr is returned.
   */
  class Val {
  private:
    Type ty;

  protected:
    /**
     * Tail end of laziness, called from eg. `output`.
     * Any result it generate (including the returned
     * pointer) are under the responsibility of `this`. No
     * free, no share.
     */
    virtual Val const* eval() const = 0;

    virtual std::ostream& output(std::ostream& out) const = 0;

  public:
    Val(Type ty): ty(ty) { }
    virtual ~Val() { }

    friend std::ostream& operator<<(std::ostream& out, Val const& val) { return val.output(out); }

    /**
     * Return a reference to the `Type` this value
     * contains. Do not free, do not share.
     */
    Type const& typed() const { return ty; }

    /**
     * Coerse to a target type (potentially different). The
     * returned object owns the previous value (or has
     * already freed it). As such:
     * - holding a ref/ptr to previous `this` is undefined
     * - previous `this` is sure to be del with new `this`
     * 
     * For Example, `Str` to `[Num]` wraps the string
     * into an iterator and packs it into a `Lst`. The
     * underlying `Str` is still in use.
     * 
     * An invalide coersion returns `nullptr` (YYY: rather
     * than throwing? wouldn't this be more C++?).
     */
    virtual Val const* coerse(Type to) const = 0;

    /**
     * Unchecked type conversion, return `this`. Reminder
     * to check type with `typed` and use `coerse`
     * if needed.
     */
    // template <typename T>
    // T const* as() const { return (T*)this; }
  }; // class Val


  class Num : public Val {
  private:
    float n;

  protected:
    Val const* eval() const override { return this; }
    std::ostream& output(std::ostream& out) const override { return out << n; }

  public:
    Num(float f): Val(BasicType::NUM), n(f) { }
    ~Num() { std::cerr << "~Num()" << std::endl; }

    Val const* coerse(Type to) const override;
  }; // class Num


  class Str : public Val {
  protected:
    Val const* eval() const override { TODO("Str::eval"); return nullptr; }
    std::ostream& output(std::ostream& out) const override { TODO("Str::output"); return out; }

  public:
    Str(): Val(BasicType::STR) { }
    Str(std::istream* in): Val(BasicType::STR) {
      std::cerr
        << "Str value initialized with:" << std::endl
        << in << std::endl
        << "---" << std::endl
      ;
    }
    ~Str() { std::cerr << "~Str()" << std::endl; }

    Val const* coerse(Type to) const override;
  }; // class Str

} // namespace sel

#endif // SEL_VALUE_HPP
