#ifndef SEL_BIDOOF_HPP
#define SEL_BIDOOF_HPP

#include "types.hpp"
#include "engine.hpp"
#include "visitors.hpp"

namespace sel {

  class Bidoof : public Num, public Str, public Lst, public Fun, public Cpl {
  private:
    std::string* a;
    Bidoof(std::string* a)
      : Str(TyFlag::IS_FIN)
      , Lst(unkType(a), TyFlag::IS_INF)
      , Fun(unkType(a), unkType(a))
      , Cpl(unkType(a), unkType(a))
      , a(a)
    { }
  public:
    Bidoof(std::string a)
      : Bidoof(new std::string(a))
    { }
    ~Bidoof() { delete a; } // XXX: note: this will be a penta-free (more like 7, but)

    double value() override { return 399; }
    std::ostream& stream(std::ostream& out) override { return out << a; }

    void rewind() override { }

    Val* operator*() override { return (Lst*)this; }
    Lst* operator++() override { return (Lst*)this; }
    bool end() const override { return false; }

    Val* operator()(Val* arg) override { return (Fun*)this; }

    Val* first() override { return (Cpl*)this; }
    Val* second() override { return (Cpl*)this; }

    void accept(Visitor& v) const override { v.visitBidoof(unkType(a)); }
  };

}

#endif // SEL_BIDOOF_HPP
