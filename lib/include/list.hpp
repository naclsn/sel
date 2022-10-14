#ifndef SEL_LIST_HPP
#define SEL_LIST_HPP

#include "value.hpp"

namespace sel {

  class Lst : public Val {
  protected:
    std::ostream& output(std::ostream& out) override;

  public:
    Lst(Type* has): Val(new Type(Ty::LST, has)) { }
    ~Lst() {
      TRACE(~Lst);
    }

    Val* coerse(Type to) override;

    /**
     * Get the next value or nullptr. Note that the
     * returned object still belong to the list. Clone
     * if needed.
     */
    virtual Val& next() = 0;
    virtual bool end() = 0;
    virtual void rewind() = 0;

    /**
     * Does not necessarily check bounds.
     */
    virtual Val& operator[](size_t n);

    virtual bool isFinite() = 0;
    virtual size_t count() = 0;
  }; // class Lst

} // namespace sel

#endif // SEL_LIST_HPP
