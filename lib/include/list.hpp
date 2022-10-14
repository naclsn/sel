#ifndef SEL_LIST_HPP
#define SEL_LIST_HPP

#include "value.hpp"

namespace sel {

  class Lst : public Val {
  protected:
    std::ostream& output(std::ostream& out) override {
      eval();
      char const* sep = Ty::LST == typed().pars.pair[0]->base
        ? Ty::LST == typed().pars.pair[0]->pars.pair[0]->base
          ? "\n\n"
          : "\n"
        : " ";
      bool first = true;
      while (!end())
        out << (first ? "" : sep) << next();
      return out;
    }

  public:
    Lst(Type has): Val(Type(Ty::LST, &has)) { }

    Val* coerse(Type to) override;

    /**
     * Get the next value or nullptr. Note that the
     * returned object still belong to the list. Clone
     * if needed.
     */
    virtual Val& next() = 0;
    virtual bool end() = 0;
    virtual void rewind() = 0;

    virtual Val* operator[](size_t n) {
      rewind();
      for (size_t k = 0; k < n; k++) next();
      return next().clone();
    }

    virtual bool isFinite() = 0;
    virtual size_t count() = 0;
  }; // class Lst

} // namespace sel

#endif // SEL_LIST_HPP
