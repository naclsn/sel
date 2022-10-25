// place holder file
#include "sel/errors.hpp" // ZZZ: NIError
#include "sel/visitors.hpp"
#include "prelude.hpp"

namespace sel {

  Val* lookup_name(Env& env, std::string const& name) {
    if ("abs"   == name) return new Abs1(env);
    if ("add"   == name) return new Add2(env);
    if ("join"  == name) return new Join2(env);
    if ("map"   == name) return new Map2(env);
    if ("split" == name) return new Split2(env);
    if ("tonum" == name) return new Tonum1(env);
    if ("tostr" == name) return new Tostr1(env);
    return nullptr;
  }


  Val* Abs1::operator()(Val* arg) { return new Abs0(env, this, coerse<Num>(arg)); }
  void Abs1::accept(Visitor& v) const { v.visitAbs1(ty); }

  double Abs0::value() {
    return std::abs(arg->value());
  }
  void Abs0::accept(Visitor& v) const { v.visitAbs0(ty, base, arg); }


  Val* Add2::operator()(Val* arg) { return new Add1(env, this, coerse<Num>(arg)); }
  void Add2::accept(Visitor& v) const { v.visitAdd2(ty); }

  Val* Add1::operator()(Val* arg) { return new Add0(env, this, coerse<Num>(arg)); }
  void Add1::accept(Visitor& v) const { v.visitAdd1(ty, base, arg); }

  double Add0::value() {
    return base->arg->value() + arg->value();
  }
  void Add0::accept(Visitor& v) const { v.visitAdd0(ty, base, arg); }


  Val* Join2::operator()(Val* arg) { return new Join1(env, this, coerse<Str>(arg)); }
  void Join2::accept(Visitor& v) const { v.visitJoin2(ty); }

  Val* Join1::operator()(Val* arg) { return new Join0(env, this, coerse<Lst>(arg)); }
  void Join1::accept(Visitor& v) const { v.visitJoin1(ty, base, arg); }

  std::ostream& Join0::stream(std::ostream& out) {
    Str& sep = *base->arg;
    Lst& lst = *arg;
    if (beginning) beginning = false;
    else sep.entire(out);
    Str* it = (Str*)*lst++;
    return it->entire(out);
  }
  bool Join0::end() const {
    Lst& lst = *arg;
    return lst.end();
  }
  void Join0::rewind() {
    Lst& lst = *arg;
    lst.rewind();
    beginning = true;
  }
  std::ostream& Join0::entire(std::ostream& out) {
    Str& sep = *base->arg;
    Lst& lst = *arg;
    if (lst.end()) return out;
    out << *lst;
    while (!lst.end()) {
      sep.rewind();
      sep.entire(out);
      out << *(Str*)*(++lst);
    }
    return out;
  }
  void Join0::accept(Visitor& v) const { v.visitJoin0(ty, base, arg); }


  Val* Map2::operator()(Val* arg) { return new Map1(env, this, coerse<Fun>(arg)); }
  void Map2::accept(Visitor& v) const { v.visitMap2(ty); }

  Val* Map1::operator()(Val* arg) { return new Map0(env, this, coerse<Lst>(arg)); }
  void Map1::accept(Visitor& v) const { v.visitMap1(ty, base, arg); }

  Val* Map0::operator*() {
    Fun& fun = *base->arg;
    Lst& lst = *arg;
    return fun(*lst);
  }
  Lst& Map0::operator++() {
    Lst& lst = *arg;
    lst++;
    return *this;
  }
  bool Map0::end() const {
    Lst& lst = *arg;
    return lst.end();
  }
  void Map0::rewind() {
    Lst& lst = *arg;
    lst.rewind();
  }
  size_t Map0::count() {
    Lst& lst = *arg;
    return lst.count();
  }
  void Map0::accept(Visitor& v) const { v.visitMap0(ty, base, arg); }


  Val* Split2::operator()(Val* arg) { return new Split1(env, this, coerse<Str>(arg)); }
  void Split2::accept(Visitor& v) const { v.visitSplit2(ty); }

  Val* Split1::operator()(Val* arg) { return new Split0(env, this, coerse<Str>(arg)); }
  void Split1::accept(Visitor& v) const { v.visitSplit1(ty, base, arg); }

  Val* Split0::operator*() {
    throw NIYError("Val* Split0::operator*()", "- what -");
    // Str& sep = *base->arg;
    // Str& str = *arg;
    return nullptr;
  }
  Lst& Split0::operator++() {
    throw NIYError("Lst& Split0::operator++()", "- what -");
    // Str& sep = *base->arg;
    // Str& str = *arg;
    return *this;
  }
  bool Split0::end() const {
    Str& str = *arg;
    return str.end();
  }
  void Split0::rewind() {
    Str& str = *arg;
    str.rewind();
  }
  size_t Split0::count() {
    throw NIYError("size_t Split0::count()", "- what -");
    return 0;
  }
  void Split0::accept(Visitor& v) const { v.visitSplit0(ty, base, arg); }


  Val* Tonum1::operator()(Val* arg) { return new Tonum0(env, this, coerse<Str>(arg)); }
  void Tonum1::accept(Visitor& v) const { v.visitTonum1(ty); }

  double Tonum0::value() {
    // XXX: quickly hacked
    std::stringstream ss;
    arg->entire(ss);
    double r;
    ss >> r;
    return r;
  }
  void Tonum0::accept(Visitor& v) const { v.visitTonum0(ty, base, arg); }


  Val* Tostr1::operator()(Val* arg) { return new Tostr0(env, this, coerse<Num>(arg)); }
  void Tostr1::accept(Visitor& v) const { v.visitTostr1(ty); }

  std::ostream& Tostr0::stream(std::ostream& out) { read = true; return out << arg->value(); }
  bool Tostr0::end() const { return read; }
  void Tostr0::rewind() { read = false; }
  std::ostream& Tostr0::entire(std::ostream& out) { read = true; return out << arg->value(); }
  void Tostr0::accept(Visitor& v) const { v.visitTostr0(ty, base, arg); }

} // namespace sel
