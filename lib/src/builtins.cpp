#include "sel/builtins.hpp"

namespace sel {

  struct Add : bin_val<Add, Ty::NUM, Ty::NUM, Ty::NUM>::tail_vat {
    constexpr static char const* name = "add"; // I have no idea why it works..? (does it?)
    double value() override { return 0; }
  };

  Val* lookup_name(std::string const& name) {
    // if ("abs"   == name) return new Abs1();
    if ("add"   == name) return new Add::head();
    // if ("flip"  == name) return new Flip2();
    // if ("join"  == name) return new Join2();
    // if ("map"   == name) return new Map2();
    // if ("split" == name) return new Split2();
    // if ("sub"   == name) return new Sub2();
    // if ("tonum" == name) return new Tonum1();
    // if ("tostr" == name) return new Tostr1();
    return nullptr;
  }

} // namespace sel
