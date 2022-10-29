#include "sel/builtins.hpp"
#include <typeinfo>

namespace sel {

  class bidoof : public VisitorBinsAll {
    void visit(bin::Add&) override { }
    void visit(bin::Add::Base&) override { }
    void visit(bin::Sub&) override { }
  };

  Val* lookup_name(std::string const& name) {
    // std::cerr << "coucou: " << typeid(idk).name() << std::endl;
    // if ("abs"   == name) return new Abs1();
    if ("add"   == name) return new bin::Add::Head();
    // if ("flip"  == name) return new Flip2();
    // if ("join"  == name) return new Join2();
    // if ("map"   == name) return new Map2();
    // if ("split" == name) return new Split2();
    if ("sub"   == name) return new bin::Sub::Head();
    // if ("tonum" == name) return new Tonum1();
    // if ("tostr" == name) return new Tostr1();
    return nullptr;
  }

} // namespace sel
