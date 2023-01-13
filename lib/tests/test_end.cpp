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
void dotestone(char const* txt, T* it, size_t n) {
  cout << txt << ", size: " << n << endl;
  assert_eq(n, count(*it));
}

template <typename T> void dotest012(char const*);
template <>
void dotest012<LstLiteral>(char const* txt) {
  dotestone(txt, (Lst*)new LstLiteral(app, {}), 0);
  dotestone(txt, (Lst*)new LstLiteral(app, {new NumLiteral(app, 0)}), 1);
  dotestone(txt, (Lst*)new LstLiteral(app, {new NumLiteral(app, 1), new NumLiteral(app, 2)}), 2);
}
template <>
void dotest012<StrChunks>(char const* txt) {
  dotestone(txt, (Str*)new StrChunks(app, vector<string>{}), 0);
  dotestone(txt, (Str*)new StrChunks(app, vector<string>{"zero"}), 1);
  dotestone(txt, (Str*)new StrChunks(app, vector<string>{"one", "two"}), 2);
}

TEST(end) {
  dotest012<LstLiteral>("LstLiteral");
  dotest012<StrChunks>("StrChunks");
}
