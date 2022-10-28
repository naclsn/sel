#ifndef SEL_BUILTINS_HPP
#define SEL_BUILTINS_HPP

// #include <string>
// #include <typeinfo>

#include "sel/visitors.hpp"
#include "sel/utils.hpp"
#include "sel/engine.hpp"

namespace sel {

  /**
   * Seach for a value by name, return nullptr if not found.
   */
  Val* lookup_name(std::string const& name);

  Val* bidoof();

  template <Ty Type> struct bin_vat { typedef Val vat; };
  template <> struct bin_vat<Ty::NUM> { typedef Num vat; };
  template <> struct bin_vat<Ty::STR> { typedef Str vat; };
  template <> struct bin_vat<Ty::LST> { typedef Lst vat; };
  template <> struct bin_vat<Ty::FUN> { typedef Fun vat; };

  /*
    ->: next
    <-: base

    head <-> body <..> body <-> tail

    head: empty ctor
    body and tail: ctor with base and arg

    head and body: extend from Fun
    tail: extends from the last type
  */
  // TODO: replace the use of raw Ty for types that hold more info
  // - need to be able to have eg. Type(Ty::LST, !!, !!)
  // - isn't there a weird syntax like `RetType(ParamType1, ..)` that could be used?

  template <typename Next, Ty From, Ty... To>
  struct bin_val : Fun {
    constexpr static char const* name = Next::name;
    typedef bin_val<bin_val, To...> Base;
    typedef typename Base::tail_vat tail_vat;
    // this is the ctor for body types
    bin_val()
      : Fun(Type(Ty::NUM, {0}, 0)) // ZZZ: wrong
    { }
    Base* base;
    Val* arg;
    inline void setup(Base* base, Val* arg) {
      this->base = base;
      this->arg = arg;
    }
    void accept(Visitor& v) const override {
      v.visitBody(Next::name, this->ty, this->base, this->arg);
    }
    Val* operator()(Val* arg) override {
      auto* r = new Next();
      r->setup(this, arg);
      return r;
    }
  };

  template <typename Next, Ty LastFrom, Ty LastTo>
  struct bin_val<Next, LastFrom, LastTo> : Fun {
    // this is the parent class for the tail type
    struct tail_vat : bin_vat<LastTo>::vat {
      typedef bin_val head;
      // this is the ctor for the tail type
      tail_vat()
        : bin_vat<LastTo>::vat() // ZZZ: wrong
      { }
      Next* base;
      Val* arg;
      inline void setup(Next* base, Val* arg) { // (this needed because wasn't able to inherit ctor)
        this->base = base;
        this->arg = arg;
      }
      void accept(Visitor& v) const override {
        v.visitTail(Next::name, this->ty, this->base, this->arg);
      }
    };
    // this is the ctor for the head type
    bin_val()
      : Fun(Type(Ty::NUM, {0}, 0)) // ZZZ: wrong
    { }
    Val* operator()(Val* arg) override {
      auto* r = new Next();
      r->setup(this, arg);
      return r;
    }
    void accept(Visitor& v) const override {
      v.visitHead(Next::name, ty);
    }
  };

} // namespace sel

#endif // SEL_BUILTINS_HPP
