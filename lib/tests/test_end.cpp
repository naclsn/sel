#include "common.hpp"

template <typename T> long unsigned count(T&);
template <>
long unsigned count<Lst>(Lst& l) {
  VisRepr repr(cout);
  size_t r = 0;
  while (auto it = ++l) {
    r++;
    it->accept(repr);
  }
  return r;
}
template <>
long unsigned count<Str>(Str& s) {
  size_t r = 0;
  while (!s.end()) {
    r++;
    cout << "_" << s << "_\n";
  }
  return r;
}

template <typename T>
int dotestone(char const* txt, unique_ptr<T> it, size_t n) {
  cout << txt << ", size: " << n << endl;
  assert_eq(n, count(*it));
  return 0;
}

template <typename T> int dotest012(char const*);
template <>
int dotest012<LstLiteral>(char const* txt) {
  return
  dotestone(txt, val_cast<Lst>(make_unique<LstLiteral>(Vals{})), 0)+
  dotestone(txt, val_cast<Lst>(make_unique<LstLiteral>(Vals{make_unique<NumLiteral>(0)})), 1)+
  dotestone(txt, val_cast<Lst>(make_unique<LstLiteral>(Vals{make_unique<NumLiteral>(1), make_unique<NumLiteral>(2)})), 2)+
  0;
}
template <>
int dotest012<StrChunks>(char const* txt) {
  return
  dotestone(txt, val_cast<Str>(make_unique<StrChunks>(vector<string>{})), 0)+
  dotestone(txt, val_cast<Str>(make_unique<StrChunks>(vector<string>{"zero"})), 1)+
  dotestone(txt, val_cast<Str>(make_unique<StrChunks>(vector<string>{"one", "two"})), 2)+
  0;
}

TEST(end) {
  return
  dotest012<LstLiteral>("LstLiteral")+
  dotest012<StrChunks>("StrChunks")+
  0;
}
