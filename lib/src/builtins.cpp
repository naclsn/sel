#include "sel/builtins.hpp"
#include <typeinfo>

namespace sel {

  struct Add : bin_val<Add, Ty::NUM, Ty::NUM, Ty::NUM>::Tail {
    double value() override {
      return ((Num*)base->arg)->value() + ((Num*)arg)->value();
    }
  };

  struct Sub : bin_val<Add, Ty::NUM, Ty::NUM, Ty::NUM>::Tail {
    double value() override {
      return ((Num*)base->arg)->value() - ((Num*)arg)->value();
    }
  };

  template <typename... T> struct bidoof { };

  template <typename TailL>
  struct bidoof<TailL> : bidoof<typename TailL::Base> {
    using bidoof<typename TailL::Base>::visit;
    virtual void visit(TailL& val) { std::cerr << "visiting: " << typeid(val).name() << '\n'; }
  };
  template <typename Next, Ty LastFrom, Ty LastTo>
  struct bidoof<bin_val<Next, LastFrom, LastTo>> {
    virtual ~bidoof() { }
    virtual void visit(bin_val<Next, LastFrom, LastTo>& val) { std::cerr << "visiting: " << typeid(val).name() << '\n'; }
  };

  template <typename TailH, typename... TailT>
  struct bidoof<TailH, TailT...> : bidoof<typename TailH::Base, TailT...> {
    using bidoof<typename TailH::Base, TailT...>::visit;
    virtual void visit(TailH& val) { std::cerr << "visiting: " << typeid(val).name() << '\n'; }
  };
  template <typename Next, Ty LastFrom, Ty LastTo, typename... TailT>
  struct bidoof<bin_val<Next, LastFrom, LastTo>, TailT...> : bidoof<TailT...> {
    using bidoof<TailT...>::visit;
    virtual void visit(bin_val<Next, LastFrom, LastTo>& val) { std::cerr << "visiting: " << typeid(val).name() << '\n'; }
  };

  struct bibarel : bidoof<Add, Sub> {
    void visit(Add&) override { }
    void visit(Add::Base&) override { }
    void visit(Sub&) override { }
  };

  Val* lookup_name(std::string const& name) {
    // std::cerr << "coucou: " << typeid(idk).name() << std::endl;
    // if ("abs"   == name) return new Abs1();
    if ("add"   == name) return new Add::Head();
    // if ("flip"  == name) return new Flip2();
    // if ("join"  == name) return new Join2();
    // if ("map"   == name) return new Map2();
    // if ("split" == name) return new Split2();
    if ("sub"   == name) return new Sub::Head();
    // if ("tonum" == name) return new Tonum1();
    // if ("tostr" == name) return new Tostr1();
    return nullptr;
  }

} // namespace sel
