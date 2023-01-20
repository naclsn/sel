#include "common.hpp"

App app;

template <typename T> long unsigned count(T&);
template <>
long unsigned count<Lst>(Lst& l) {
  VisRepr repr(cout);
  size_t r = 0;
  while (!l.end()) {
    r++;
    repr(**l);
    ++l;
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
int dotestone(char const* txt, T* it, size_t n) {
  cout << txt << ", size: " << n << endl;
  assert_eq(n, count(*it));
  return 0;
}

template <typename T> int dotest012(char const*);
template <>
int dotest012<LstLiteral>(char const* txt) {
  return
  dotestone(txt, (Lst*)new LstLiteral(app, {}), 0)+
  dotestone(txt, (Lst*)new LstLiteral(app, {new NumLiteral(app, 0)}), 1)+
  dotestone(txt, (Lst*)new LstLiteral(app, {new NumLiteral(app, 1), new NumLiteral(app, 2)}), 2)+
  0;
}
template <>
int dotest012<StrChunks>(char const* txt) {
  return
  dotestone(txt, (Str*)new StrChunks(app, vector<string>{}), 0)+
  dotestone(txt, (Str*)new StrChunks(app, vector<string>{"zero"}), 1)+
  dotestone(txt, (Str*)new StrChunks(app, vector<string>{"one", "two"}), 2)+
  0;
}

TEST(end) {
  return
  dotest012<LstLiteral>("LstLiteral")+
  dotest012<StrChunks>("StrChunks")+
  0;
}
