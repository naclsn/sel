#include "sel/builtins.hpp"
#include "sel/visitors.hpp"

namespace sel {

  Val* lookup_name(std::string const& name) {
    // if ("abs"   == name) return new Abs1();
    if ("add"   == name) return new bin::Add::Head();
    // if ("flip"  == name) return new Flip2();
    // if ("idk"   == name) return new bin::Idk::Head();
    // if ("join"  == name) return new Join2();
    // if ("map"   == name) return new Map2();
    // if ("split" == name) return new Split2();
    // if ("sub"   == name) return new bin::Sub::Head();
    // if ("tonum" == name) return new Tonum1();
    // if ("tostr" == name) return new Tostr1();
    return nullptr;
  }

  template <typename NextT, typename From, typename... To>
  void bin_val_helpers::bin_val<NextT, From, To...>::accept(Visitor& v) const {
    v.visit(*this); // visitBody
  }

  template <typename NextT, typename LastFrom, typename LastTo>
  void bin_val_helpers::bin_val<NextT, LastFrom, LastTo>::the::accept(Visitor& v) const {
    v.visit(*(typename Base::Next*)this); // visitTail
  };

  template <typename NextT, typename LastFrom, typename LastTo>
  void bin_val_helpers::bin_val<NextT, LastFrom, LastTo>::accept(Visitor& v) const {
    v.visit(*this); // visitHead
  }

} // namespace sel
