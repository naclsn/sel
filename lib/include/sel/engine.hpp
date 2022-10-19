#ifndef SEL_ENGINE_HPP
#define SEL_ENGINE_HPP

/**
 * Main classes involved an runtime: types of values and
 * the application.
 */

#include <ostream>

#include "types.hpp"

namespace sel {

  class Visitor;

  /**
   * Abstract base class for every types of values. See
   * also `Num`, `Str`, `Lst`, `Fun` and `Cpl`. Each of
   * these abstract class in turn describe the common
   * ground for values of a type.
   */
  class Val {
  protected:
    Type const ty;
    // virtual void eval() = 0; // more of a friendly reminder than actual contract
  public:
    // Val(Val const& val) { }
    Val(Type&& ty): ty(std::move(ty)) { }
    virtual ~Val() { }
    Type const& type() const { return ty; }
    virtual void accept(Visitor& v) const = 0;
  };

  /**
   * Abstract class for `Num`-type compatible values.
   * Numbers can be converted to common representation
   * (double for now).
   */
  class Num : public Val {
  public:
    Num()
      : Val(numType())
    { }
    virtual double value() = 0;
  };

  /**
   * Abstract class for `Str`-type compatible values.
   * Strings are "rewindable streams".
   */
  class Str : public Val { //, public std::istream
  public:
    Str(TyFlag is_inf)
      : Val(strType(is_inf))
    { }
    friend std::ostream& operator<<(std::ostream& out, Str& val) { return val.output(out); }
    /**
     * Stream (compute, etc..) some bytes.
     */
    virtual std::ostream& output(std::ostream& out) = 0;
    /**
     * Resets the internal state so the string may be
     * streamed again.
     */
    virtual void rewind() = 0;
  };

  /**
   * Abstract class for `Lst`-type compatible values.
   * Lists are more like "rewindable iterators". (Methods
   * `next` and `rewind`.)
   */
  class Lst : public Val { //, public std::iterator<std::input_iterator_tag, Val>
  public:
    Lst(Type&& has, TyFlag is_inf)
      : Val(lstType(new Type(std::move(has)), is_inf))
    { }
    /**
     * Get (compute, etc..) the current value.
     */
    virtual Val& operator*() = 0;
    /**
     * Move to (compute, etc..) the next value.
     */
    virtual Lst* operator++() = 0;
    /**
     * `true` if there is no next value to get. Calling
     * `next` at this point is probably undefined.
     */
    virtual bool end() const = 0;
    /**
     * Resets the internal state so the list may be
     * interated again.
     */
    virtual void rewind() = 0;
    // virtual size_t cound() = 0; // more like a size hint than actual count
    // virtual Val& operator[](size_t n) { rewind(); for (;;); return ...; }
  };

  /**
   * Abstract class for `Fun`-type compatible values.
   * Functions can be applied an argument to produce
   * a new value.
   */
  class Fun : public Val {
  public:
    Fun(Type&& fst, Type&& snd)
      : Val(funType(new Type(std::move(fst)), new Type(std::move(snd))))
    { }
    virtual Val* operator()(Val* arg) = 0;
  };

  /**
   * Abstract class for `Cpl`-type compatible values.
   * Couples have a `first` and a `second`. Yeah, that's
   * pretty much it. (Althrough these are often refered
   * to as 'fst' and 'snd'.)
   */
  class Cpl : public Val {
  public:
    Cpl(Type&& fst, Type&& snd)
      : Val(cplType(new Type(std::move(fst)), new Type(std::move(snd))))
    { }
    virtual Val& first() = 0;
    virtual Val& second() = 0;
  };

  /**
   * An application is constructed from parsing a user
   * script. It serializes back to an equivalent script
   * (although it may not be strictly equal).
   */
  class Application {
  };

} // namespace sel

#endif // SEL_ENGINE_HPP
