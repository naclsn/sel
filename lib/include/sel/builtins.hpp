#ifndef SEL_BUILTINS_HPP
#define SEL_BUILTINS_HPP

#include <string>

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

  template <typename Next, Ty From, Ty... To>
  struct bin_val : Fun {
    typedef bin_val<bin_val, To...> Base;
    typedef typename Base::tail_vat tail_vat;
    // this is the ctor for body types
    bin_val()
      : Fun(Type(Ty::NUM, {0}, 0))
    { }
    void setup(Base* base, Val* arg) { }
    Val* operator()(Val* arg) {
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
        : bin_vat<LastTo>::vat()
      { }
      void setup(Next* base, Val* arg) { }
    };
    // this is the ctor for the head type
    bin_val()
      : Fun(Type(Ty::NUM, {0}, 0))
    { }
    Val* operator()(Val* arg) {
      auto* r = new Next();
      r->setup(this, arg);
      return r;
    }
  };

} // namespace sel

#endif // SEL_BUILTINS_HPP
