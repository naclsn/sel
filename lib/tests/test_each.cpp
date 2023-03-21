#include "common.hpp"
using namespace bins;

// completely disables this test when working with the `bins_min` restricted subset
#ifdef BINS_MIN
TEST(each) { cout << "test disable as BIN_MIN is defined\n"; return 0; }
#else

template <typename T> inline unique_ptr<Val> asval(T x) { return x; }

template <> inline unique_ptr<Val> asval(int x) { return make_unique<NumLiteral>(x); }
template <> inline unique_ptr<Val> asval(float x) { return make_unique<NumLiteral>(x); }
template <> inline unique_ptr<Val> asval(double x) { return make_unique<NumLiteral>(x); }

template <> inline unique_ptr<Val> asval(char x) { return make_unique<StrLiteral>(string(1, x)); }
template <> inline unique_ptr<Val> asval(char const* x) { return make_unique<StrLiteral>(x); }
template <> inline unique_ptr<Val> asval(string x) { return make_unique<StrLiteral>(move(x)); }

template <typename T> inline unique_ptr<Val> _lst_asval(initializer_list<T> x, Type const& ty) {
  Vals r;
  for (auto const& it : x)
    r.push_back(asval(it));
  return make_unique<LstLiteral>(move(r), Types{Type(ty)});
}
template <> inline unique_ptr<Val> asval(initializer_list<int> x) { return _lst_asval(x, Type::makeNum()); }
template <> inline unique_ptr<Val> asval(initializer_list<float> x) { return _lst_asval(x, Type::makeNum()); }
template <> inline unique_ptr<Val> asval(initializer_list<char> x) { return _lst_asval(x, Type::makeStr(false)); }
template <> inline unique_ptr<Val> asval(initializer_list<char const*> x) { return _lst_asval(x, Type::makeStr(false)); }
template <> inline unique_ptr<Val> asval(initializer_list<string> x) { return _lst_asval(x, Type::makeStr(false)); }

template <typename T>
using ili = initializer_list<T>;


template <typename ...L> struct uncurry;

template <typename H, typename ...T>
struct uncurry<H, T...> {
  static inline unique_ptr<Val> function(unique_ptr<Val> f, H h, T... t) {
    return uncurry<T...>::function((*val_cast<Fun>(move(f)))(asval(forward<H>(h))), t...);
  }
};

template <>
struct uncurry<> {
  static inline unique_ptr<Val> function(unique_ptr<Val> v) {
    return v;
  }
};

template <typename F, typename ...Args>
static inline unique_ptr<Val> call(Args... args) {
  return uncurry<Args...>::function(unique_ptr<typename F::Head>(), forward<Args>(args)...);
}


struct test_base {
  virtual ~test_base() { }
  operator int() {
    cerr << "--- [test for '" << get_name() << "']\n";
    int r = run_test();
    if (r) cerr << "--> in test for '" << get_name() << "'\n";
    return r;
  }
  virtual int run_test() = 0;
  virtual char const* get_name() = 0;
};

template <typename F>
struct test : test_base {
  /** return true when failed */
  int run_test() override { cout << "no test for '" << F::name << "'\n"; return 0; }
  char const* get_name() override { return F::name; }
};


template <typename PackItself> struct call_test;
template <typename ...Pack>
struct call_test<ll::pack<Pack...>> {
  static inline int function() {
    std::initializer_list<int> l{test<Pack>()...};
    return std::accumulate(l.begin(), l.end(), 0);
  }
};

TEST(each) { return call_test<bins_list::all>::function(); }


#define T(__f)                         \
  template <>                          \
  struct test<__f> : test_base {       \
    int run_test() override;           \
    char const* get_name() override {  \
      return __f::name;                \
    };                                 \
  };                                   \
  int test<__f>::run_test()

#define __rem_par(...) __VA_ARGS__

#define assert_num(__should, __have) do {   \
  auto _num = val_cast<Num>(__have);        \
  assert_eq(Ty::NUM, _num->type().base());  \
  assert_eq(__should, _num->value());      \
} while (0)

#define assert_str(__should, __have) do {                \
  auto _str = val_cast<Str>(__have);                     \
  assert_eq(Ty::STR, _str->type().base());               \
  ostringstream oss;                                     \
  assert_cmp(__should, (_str->entire(oss), oss.str()));  \
} while (0)

#define assert_lstnum(__should, __have) do {                             \
  auto _lst = val_cast<Lst>(__have);                                     \
  assert_eq(Ty::LST, _lst->type().base());                               \
  for (auto const& _it : __rem_par __should) {                           \
    auto _cur = ++*_lst;                                                 \
    assert(_cur, #__have ":\n   should not have reached the end yet");   \
    assert_num(_it, move(_cur));                                         \
  }                                                                      \
  assert(!++*_lst, #__have ":\n   should have reached the end by now");  \
} while(0)

#define assert_lststr(__should, __have) do {                             \
  auto _lst = val_cast<Lst>(__have);                                     \
  assert_eq(Ty::LST, _lst->type().base());                               \
  for (auto const& _it : __rem_par __should) {                           \
    auto _cur = ++*_lst;                                                 \
    assert(_cur, #__have ":\n   should not have reached the end yet");   \
    assert_str(_it, move(_cur));                                         \
  }                                                                      \
  assert(!++*_lst, #__have ":\n   should have reached the end by now");  \
} while(0)

#define assert_empty(__have) do {                          \
  auto _lst = val_cast<Lst>(__have);                       \
  assert(!++*_lst, "should have reached the end by now");  \
} while(0)

#define assert_lsterr(__throws) do {                 \
  try {                                              \
    ++*val_cast<Lst>(__throws);                      \
  } catch (RuntimeError const&) {                    \
    break;                                           \
  }                                                  \
  fail("expected " #__throws " to throw an error");  \
} while(0)


T(abs_) {
  assert_num(5, call<abs_>(-5));
  assert_num(0, call<abs_>(0));
  assert_num(3, call<abs_>(+3));
  return 0;
}

T(add_) {
  assert_num(6, call<add_>(4, 2));
  return 0;
}

T(bin_) {
  assert_str("101010", call<bin_>(42));
  assert_str("0", call<bin_>(0));
  return 0;
}

T(bytes_) {
  assert_lstnum(
    ({ 97, 227, 129, 181, 98, 13, 10, 99, 240, 159, 143, 179, 226, 128, 141, 226, 154, 167, 100 }),
    call<bytes_>("\x61\xe3\x81\xb5\x62\x0d\x0a\x63\xf0\x9f\x8f\xb3\xe2\x80\x8d\xe2\x9a\xa7\x64")
  );
  // assert_lstnum(
  //   ({ 97, 227, 129, 181, 98, 13, 10, 99, 240, 159, 143, 179, 226, 128, 141, 226, 154, 167, 100 }),
  //   call<bytes_>("\x61\xe3\x81\xb5\x62\x0d\x0a\x63\xf0\x9f\x8f\xb3\xe2\x80\x8d\xe2\x9a\xa7\x64")
  // );
  assert_empty(call<bytes_>(""));
  return 0;
}

T(chr_) {
  assert_str("*", call<chr_>(42));
  return 0;
}

T(codepoints_) {
  assert_lstnum(
    ({ 97, 12405, 98, 13, 10, 99, 127987, 8205, 9895, 100 }),
    call<codepoints_>("\x61\xe3\x81\xb5\x62\x0d\x0a\x63\xf0\x9f\x8f\xb3\xe2\x80\x8d\xe2\x9a\xa7\x64")
  );
  assert_empty(call<codepoints_>(""));
  return 0;
}

T(const_) {
  assert_str("coucou", call<const_>("coucou", 1));
  return 0;
}

T(contains_) {
  assert_num(1, call<contains_>("abc", "abc"));
  assert_num(1, call<contains_>("a", "abc"));
  assert_num(1, call<contains_>("b", "abc"));
  assert_num(1, call<contains_>("c", "abc"));
  assert_num(1, call<contains_>("", "abc"));
  assert_num(0, call<contains_>("abcz", "abc"));
  assert_num(0, call<contains_>("zabc", "abc"));
  assert_num(0, call<contains_>("zabcz", "abc"));
  assert_num(0, call<contains_>("z", "abc"));
  return 0;
}

T(div_) {
  assert_num(2, call<div_>(4, 2));
  return 0;
}

T(drop_) {
  assert_lstnum(({3, 2, 1}), call<drop_>(2, (ili<int>{5, 4, 3, 2, 1})));
  assert_lstnum(({5, 4, 3, 2, 1}), call<drop_>(0, (ili<int>{5, 4, 3, 2, 1})));
  assert_lstnum((ili<int>{}), call<drop_>(2, (ili<int>{})));
  assert_lstnum((ili<int>{}), call<drop_>(6, (ili<int>{5, 4, 3, 2, 1})));
  return 0;
}

T(endswith_) {
  assert_num(1, call<endswith_>("abc", "abc"));
  assert_num(0, call<endswith_>("a", "abc"));
  assert_num(0, call<endswith_>("b", "abc"));
  assert_num(1, call<endswith_>("c", "abc"));
  assert_num(1, call<endswith_>("", "abc"));
  assert_num(0, call<endswith_>("abcz", "abc"));
  assert_num(0, call<endswith_>("zabc", "abc"));
  assert_num(0, call<endswith_>("zabcz", "abc"));
  assert_num(0, call<endswith_>("z", "abc"));
  return 0;
}

T(graphemes_) {
  assert_lststr(
    ({ "\x61", "\xe3\x81\xb5", "\x62", "\x0d\x0a", "\x63", "\xf0\x9f\x8f\xb3\xe2\x80\x8d\xe2\x9a\xa7", "\x64" }),
    call<graphemes_>("\x61\xe3\x81\xb5\x62\x0d\x0a\x63\xf0\x9f\x8f\xb3\xe2\x80\x8d\xe2\x9a\xa7\x64")
  );
  assert_empty(call<graphemes_>(""));
  return 0;
}

T(head_) {
  assert_num(1, call<head_>((ili<int>{ 1, 2, 3 })));
  assert_num(1, call<head_>((ili<int>{ 1 })));
  assert_lsterr(call<head_>((ili<int>{})));
  return 0;
}

T(hex_) {
  assert_str("2a", call<hex_>(42));
  assert_str("0", call<hex_>(0));
  return 0;
}

T(id_) {
  assert_str("coucou", call<id_>("coucou"));
  return 0;
}

T(init_) {
  assert_lstnum(({ 1, 2 }), call<init_>((ili<int>{ 1, 2, 3 })));
  assert_empty(call<init_>((ili<int>{ 1 })));
  assert_lsterr(call<init_>((ili<int>{})));
  return 0;
}

T(last_) {
  assert_num(3, call<last_>((ili<int>{ 1, 2, 3 })));
  assert_num(1, call<last_>((ili<int>{ 1 })));
  assert_lsterr(call<last_>((ili<int>{})));
  return 0;
}

T(mul_) {
  assert_num(8, call<mul_>(4, 2));
  return 0;
}

T(oct_) {
  assert_str("52", call<oct_>(42));
  assert_str("0", call<oct_>(0));
  return 0;
}

T(ord_) {
  assert_num(42, call<ord_>("*"));
  assert_num(0, call<ord_>(""));
  return 0;
}

T(prefix_) {
  assert_str("abcxyz", call<prefix_>("abc", "xyz"));
  assert_str("abc", call<prefix_>("abc", ""));
  assert_str("xyz", call<prefix_>("", "xyz"));
  return 0;
}

T(startswith_) {
  assert_num(1, call<startswith_>("abc", "abc"));
  assert_num(1, call<startswith_>("a", "abc"));
  assert_num(0, call<startswith_>("b", "abc"));
  assert_num(0, call<startswith_>("c", "abc"));
  assert_num(1, call<startswith_>("", "abc"));
  assert_num(0, call<startswith_>("abcz", "abc"));
  assert_num(0, call<startswith_>("zabc", "abc"));
  assert_num(0, call<startswith_>("zabcz", "abc"));
  assert_num(0, call<startswith_>("z", "abc"));
  return 0;
}

T(sub_) {
  assert_num(2, call<sub_>(4, 2));
  return 0;
}

T(suffix_) {
  assert_str("xyzabc", call<suffix_>("abc", "xyz"));
  assert_str("abc", call<suffix_>("abc", ""));
  assert_str("xyz", call<suffix_>("", "xyz"));
  return 0;
}

T(surround_) {
  assert_str("abccoucouxyz", call<surround_>("abc", "xyz", "coucou"));
  assert_str("abccoucou", call<surround_>("abc", "", "coucou"));
  assert_str("coucouxyz", call<surround_>("", "xyz", "coucou"));
  assert_str("abcxyz", call<surround_>("abc", "xyz", ""));
  return 0;
}

T(tail_) {
  assert_lstnum(({ 2, 3 }), call<tail_>((ili<int>{ 1, 2, 3 })));
  assert_empty(call<tail_>((ili<int>{ 1 })));
  assert_lsterr(call<tail_>((ili<int>{})));
  return 0;
}

T(take_) {
  assert_lstnum(({5, 4}), call<take_>(2, (ili<int>{5, 4, 3, 2, 1})));
  assert_lstnum((ili<int>{}), call<take_>(0, (ili<int>{5, 4, 3, 2, 1})));
  assert_lstnum((ili<int>{}), call<take_>(2, (ili<int>{})));
  assert_lstnum(({5, 4, 3, 2, 1}), call<take_>(6, (ili<int>{5, 4, 3, 2, 1})));
  return 0;
}

T(tonum_) {
  assert_num(42, call<tonum_>("42"));
  assert_num(0, call<tonum_>("garbage"));
  assert_num(-1, call<tonum_>("-1"));
  assert_num(0.3, call<tonum_>("0.3"));
  // idk if that a behavior i'd wand to keep, but it's a thing for now
  assert_str("-0", call<tostr_>(call<tonum_>("-0")));
  return 0;
}

T(tostr_) {
  assert_str("42", call<tostr_>(42));
  assert_str("0", call<tostr_>(0));
  assert_str("-1", call<tostr_>(-1));
  assert_str("0.3", call<tostr_>(0.3));
  return 0;
}

T(unbin_) {
  assert_num(42, call<unbin_>("101010"));
  assert_num(0, call<unbin_>("garbage"));
  return 0;
}

T(unbytes_) {
  assert_str(
    "\x61\xe3\x81\xb5\x62\x0d\x0a\x63\xf0\x9f\x8f\xb3\xe2\x80\x8d\xe2\x9a\xa7\x64",
    call<unbytes_>((ili<int>{ 97, 227, 129, 181, 98, 13, 10, 99, 240, 159, 143, 179, 226, 128, 141, 226, 154, 167, 100 }))
  );
  assert_str("", call<unbytes_>((ili<int>{})));
  return 0;
}

T(uncodepoints_) {
  assert_str(
    "\x61\xe3\x81\xb5\x62\x0d\x0a\x63\xf0\x9f\x8f\xb3\xe2\x80\x8d\xe2\x9a\xa7\x64",
    call<uncodepoints_>((ili<int>{ 97, 12405, 98, 13, 10, 99, 127987, 8205, 9895, 100 }))
  );
  assert_str("", call<uncodepoints_>((ili<int>{})));
  return 0;
}

T(unhex_) {
  assert_num(42, call<unhex_>("2a"));
  assert_num(0, call<unhex_>("garbage"));
  return 0;
}

T(unoct_) {
  assert_num(42, call<unoct_>("52"));
  assert_num(0, call<unoct_>("garbage"));
  return 0;
}

#endif // BIN_MIN
