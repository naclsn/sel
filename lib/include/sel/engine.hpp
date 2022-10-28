#ifndef SEL_ENGINE_HPP
#define SEL_ENGINE_HPP

/**
 * Main classes involved an runtime: types of values and
 * the application.
 */

#include <ostream>
#include <vector>

#include "types.hpp"

namespace sel {

  class App;

  class Visitor;

  /**
   * Abstract base class for every types of values. See
   * also `Num`, `Str`, `Lst`, `Fun`. Each of
   * these abstract class in turn describe the common
   * ground for values of a type.
   */
  class Val {
  protected:
    Type const ty;
    // virtual void eval() = 0; // more of a friendly reminder than actual contract
  public:
    Val(Type const& ty)
      : ty(Type(ty))
    { }
    virtual ~Val() { }
    Type const& type() const { return ty; }
    virtual void accept(Visitor& v) const;
  };

  /**
   * Coerse a value to a type. Returned pointer may be
   * the same or a newly allocated value.
   * For now this is a template function, but this might
   * not be enough with dynamically known types; may change
   * it for `coerse(Val val, Type to)`.
   */
  template <typename To>
  To* coerse(Val* from); // TODO: add `Type const& ty`

  /**
   * Abstract class for `Num`-type compatible values.
   * Numbers can be converted to common representation
   * (double for now).
   */
  class Num : public Val {
  public:
    Num()
      : Val(Type(Ty::NUM, {0}, 0))
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
      : Val(Type(Ty::STR, {0}, is_inf))
    { }
    friend std::ostream& operator<<(std::ostream& out, Str& val) { return val.stream(out); }
    /**
     * Stream (compute, etc..) some bytes.
     */
    virtual std::ostream& stream(std::ostream& out) = 0;
    /**
     * `true` if there is no more bytes to stream. Calling
     * `stream` (or `entire`) at this point is probably undefined.
     */
    virtual bool end() const = 0;
    /**
     * Reset the internal state so the string may be
     * streamed again.
     */
    virtual void rewind() = 0;
    /**
     * Stream the whole string of bytes.
     */
    virtual std::ostream& entire(std::ostream& out) = 0;
  };

  /**
   * Abstract class for `Lst`-type compatible values.
   * Lists are more like "rewindable iterators". (Methods
   * `next` and `rewind`.)
   */
  class Lst : public Val { //, public std::iterator<std::input_iterator_tag, Val>
  public:
    Lst(Type const& type)
      : Val(type)
    { }
    /**
     * Get (compute, etc..) the current value.
     */
    virtual Val* operator*() = 0;
    /**
     * Move to (compute, etc..) the next value.
     */
    virtual Lst& operator++() = 0;
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
    /**
     * Only valid for a finite list of course. [More like
     * a size hint than actual count. [?]]
     */
    virtual size_t count() = 0;
    // virtual Val& operator[](size_t n) { rewind(); for (;;); return ...; }
    Lst& operator++(int) { ++*this; return *this; } // ZZZ: i dont even anymore
  };

  /**
   * Abstract class for `Fun`-type compatible values.
   * Functions can be applied an argument to produce
   * a new value.
   */
  class Fun : public Val {
  public:
    Fun(Type const& type)
      : Val(type)
    { }
    virtual Val* operator()(Val* arg) = 0;
  };

} // namespace sel

#endif // SEL_ENGINE_HPP
