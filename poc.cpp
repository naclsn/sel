#include <iostream>
// #include <typeinfo>


struct Base { };

struct a : Base { constexpr static char const* name = "a"; };
struct b : Base { constexpr static char const* name = "b"; };
struct c : Base { constexpr static char const* name = "c"; };
struct d : Base { constexpr static char const* name = "d"; };

template <typename A, typename D>
struct cons {
  typedef A car;
  typedef D cdr;
};
struct nil;

typedef cons<a, cons<b, cons<c, cons<d, nil>>>> all;


template <typename ...T> struct pack;

template <typename L, typename ...Pack>
struct packer {
  typedef typename packer<typename L::cdr, Pack..., typename L::car>::packed packed;
};
template <typename ...Pack>
struct packer<nil, Pack...> {
  typedef pack<Pack...> packed;
};

typedef packer<all>::packed all_pack;


// class VisitorExplicit {
// public:
//   virtual ~VisitorExplicit() { }
//   virtual void visit(a const&) { }
//   virtual void visit(b const&) { }
//   virtual void visit(c const&) { }
//   virtual void visit(d const&) { }
// };


// template <typename L>
// class _VisitorInherit : _VisitorInherit<typename L::cdr> {
// public:
//   using _VisitorInherit<typename L::cdr>::visit;
//   void visit(typename L::car const&);
// };
// template <>
// class _VisitorInherit<nil> {
// public:
//   void visit();
// };
// class VisitorInherit : _VisitorInherit<all> { };


template <typename O>
class VisitorOne {
public:
  virtual ~VisitorOne() { }
  virtual void visit(O const&) { std::cout << "base for " << O::name << std::endl; }
};
template <typename PackItself> class _VisitorPackAll;
template <typename ...Pack>
class _VisitorPackAll<pack<Pack...>> : public VisitorOne<Pack>... { };

class Visitor : public _VisitorPackAll<all_pack> { };


class _VisSomeRoot {
protected:
  void visitCommon() { std::cout << "override for "; }
};

template <typename O>
class _VisSomeOne : public VisitorOne<O>, protected _VisSomeRoot {
public:
  void visit(O const&) override { visitCommon(); std::cout << O::name << std::endl; }
};

template <typename PackItself> class _VisSomeAll;
template <typename ...Pack>
class _VisSomeAll<pack<Pack...>> : public _VisSomeOne<Pack>... {
public:
  using VisitorOne<Pack>::visit...; // C++17
};

class VisSome : public _VisSomeAll<all_pack> { };


using namespace std;

int main() {
  // cout << "VisitorExplicit: " << typeid(VisitorExplicit).name() << endl;
  // cout << endl;

  // cout << "VisitorInherit: " << typeid(VisitorInherit).name() << endl;
  // cout << "_VisitorInherit<all>: " << typeid(_VisitorInherit<all>).name() << endl;
  // cout << endl;

  // cout << "VisitorPack: " << typeid(VisitorPack).name() << endl;
  // cout << "_VisitorPackAll<a, b, c, d>: " << typeid(_VisitorPackAll<all_pack>).name() << endl;
  // cout << endl;

  VisSome v;
  v.visit(a());

  return 0;
}
