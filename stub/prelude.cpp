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
    return nullptr;
  }


  Val* Abs1::operator()(Val* arg) { return new Abs0(env, this, coerse<Num>(arg)); }

  double Abs0::value() {
    return std::abs(arg->value());
  }


  Val* Add2::operator()(Val* arg) { return new Add1(env, this, coerse<Num>(arg)); }
  // void Add2::visit(Visitor& v) { v.visitAdd2(ty, ); }

  Val* Add1::operator()(Val* arg) { return new Add0(env, this, coerse<Num>(arg)); }
  // void Add1::visit(Visitor& v) { v.visitAdd1(ty, ); }

  double Add0::value() {
    return base->arg->value() + arg->value();
  }
  // void Add0::visit(Visitor& v) { v.visitAdd0(ty, ); }


  Val* Join2::operator()(Val* arg) { return new Join1(env, this, coerse<Str>(arg)); }
  // void Join2::visit(Visitor& v) { v.visitJoin2(ty, ); }

  Val* Join1::operator()(Val* arg) { return new Join0(env, this, coerse<Lst>(arg)); }
  // void Join1::visit(Visitor& v) { v.visitJoin1(ty, ); }

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
  // void Join0::visit(Visitor& v) { v.visitJoin0(ty, ); }


  Val* Map2::operator()(Val* arg) { return new Map1(env, this, coerse<Fun>(arg)); }
  // void Map2::visit(Visitor& v) { v.visitMap2(ty, ); }

  Val* Map1::operator()(Val* arg) { return new Map0(env, this, coerse<Lst>(arg)); }
  // void Map1::visit(Visitor& v) { v.visitMap1(ty, ); }

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
  // void Map0::visit(Visitor& v) { v.visitMap0(ty, ); }


  Val* Split2::operator()(Val* arg) { return new Split1(env, this, coerse<Str>(arg)); }
  // void Split2::visit(Visitor& v) { v.visitSplit2(ty, ); }

  Val* Split1::operator()(Val* arg) { return new Split0(env, this, coerse<Str>(arg)); }
  // void Split1::visit(Visitor& v) { v.visitSplit1(ty, ); }

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
  // void Split0::visit(Visitor& v) { v.visitSplit0(ty, ); }

} // namespace sel
