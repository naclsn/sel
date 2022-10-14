#ifndef SEL_VALUE_HPP
#define SEL_VALUE_HPP

#include <iostream>
#include <string>
#include <sstream>
#include <assert.h>

#include "utils.hpp"

namespace sel {

  enum class Ty {
    UNK, // str
    NUM, // 0
    STR, // 0
    LST, // 1
    FUN, // 2
    CPL, // 2
  };

  struct Type {
    Ty base;
    union {
      std::string* name;
      Type* pair[2];
    } pars;

    Type(Ty ty) {
      assert(Ty::NUM == ty || Ty::STR == ty);
      base = ty;
    }
    Type(Ty ty, std::string* name) {
      assert(Ty::UNK == ty);
      base = ty;
      pars.name = name;
    }
    Type(Ty ty, Type* has) {
      assert(Ty::LST == ty);
      base = ty;
      pars.pair[0] = has;
    }
    Type(Ty ty, Type* fst, Type* snd) {
      assert(Ty::FUN == ty || Ty::CPL == ty);
      base = ty;
      pars.pair[0] = fst;
      pars.pair[1] = snd;
    }

    Type(std::istream& in) { }

    ~Type() {
      switch (base) {
        case Ty::UNK:
          delete pars.name;
          break;

        case Ty::LST:
          delete pars.pair[0];
          break;

        case Ty::FUN:
        case Ty::CPL:
          delete pars.pair[0];
          delete pars.pair[1];
          break;

        default: ;
      }
    }

    std::ostream& output(std::ostream& out);
    bool operator==(Type& other);
  }; // struct Type
  std::ostream& operator<<(std::ostream& out, Type& ty);


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
     * Tail end of laziness, called from eg. `output`. This
     * should only need to actually compute once, following
     * calls are likely to be no-ops. Any result it
     * generate (including the returned pointer) are under
     * the responsibility of `this`.
     */
    virtual void eval() = 0;

    /**
     * Produces the standard textual representation,
     * used when the script eventually needs to generate
     * an output.
     */
    virtual std::ostream& output(std::ostream& out) = 0;

  public:
    Val(Type ty): ty(ty) { }
    virtual ~Val() { }

    friend std::ostream& operator<<(std::ostream& out, Val& val)
    { return val.output(out); }

    /**
     * Return a reference to the `Type` this value
     * contains. Do not free, do not share.
     */
    Type& typed() { return ty; }

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
     * If the coersion cannot be done, throws a `CoerseError`.
     * 
     */
    virtual Val* coerse(Type to) = 0;
  }; // class Val

} // namespace sel

#endif // SEL_VALUE_HPP
