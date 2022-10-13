#include <iostream>
#include <string>

namespace Engine {

  enum class BasicType {
    NUM,
    STR,
    LST,
    FUN,
    CPL,
  };
  struct Type {
    BasicType base;
    BasicType pars[2];
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
  protected:
    virtual std::ostream& output(std::ostream& stream) const = 0;

  public:
    virtual ~Val() { }

    virtual const Num* asNum()           const { return nullptr; }
    virtual const Str* asStr()           const { return nullptr; }
    virtual const Lst* asLst(Type)       const { return nullptr; }
    virtual const Fun* asFun(Type, Type) const { return nullptr; }
    virtual const Cpl* asCpl(Type, Type) const { return nullptr; }

    virtual const Num* toNum()               const { return asNum();     }
    virtual const Str* toStr()               const { return asStr();     }
    virtual const Lst* toLst(Type a)         const { return asLst(a);    }
    virtual const Fun* toFun(Type a, Type b) const { return asFun(a, b); }
    virtual const Cpl* toCpl(Type a, Type b) const { return asCpl(a, b); }

    friend std::ostream& operator<<(std::ostream& stream, const Val& val) { return val.output(stream); }
  };

  class Num : public Val {
  private:
    float n;

  public:
    Num(float f): n(f) { }

    const Num* asNum() const override { return this; }
    const Str* toStr() const override;

    const Num operator+(const Num& other) const { return Num(n+other.n); }

  protected:
    std::ostream& output(std::ostream& stream) const override { return stream << n; }
  };

  class Str : public Val, std::string {
  public:
    Str(const char* c): std::string(c) { }
    Str(std::string s): std::string(s) { }

    const Str* asStr() const override { return this; }
    const Num* toNum() const override;
    // const Lst* toLst(Type) const override;

  protected:
    std::ostream& output(std::ostream& stream) const override { return stream << this; }
  };

  class Lst : public Val {
  public:
    Lst() { }

    // const Lst* asNumLst() const override { return this; }
    // const Lst* asStrLst() const override { return this; }

  protected:
    std::ostream& output(std::ostream& stream) const override { return stream << "@{}"; }
  };

}
