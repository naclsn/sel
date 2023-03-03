#include "common.hpp"

int pushAssert(vector<Type>& all, Type const& push, string const expect) {
  all.push_back(push);

  ostringstream oss;
  oss << all[all.size()-1];

  cout << "---\t" << all[all.size()-1] << '\n';
  assert_cmp(expect, oss.str());

  return 0;
}

int doApplied(Type fun, vector<Type> args, vector<string> reprx) {
  vector<Type> all;
  int fails = 0;

  fails+= pushAssert(all, fun, reprx[0]);

  for (size_t k = 0; k < args.size(); k++) {
    cout << all[all.size()-1].from() << "  <-  " << args[k] << '\n';

    Type ty;
    all[all.size()-1].applied(args[k], ty);
    fails+= pushAssert(all, ty, reprx[k+1]);
  }

  return fails;
}

TEST(Type_applied) {
  int fails = 0;

  Type map2_t = Type::makeFun(
    Type::makeFun(
      Type::makeUnk("a_map"),
      Type::makeUnk("b_map")
    ),
    Type::makeFun(
      Type::makeLst({Type::makeUnk("a_map")}, true, false),
      Type::makeLst({Type::makeUnk("b_map")}, true, false)
    )
  );

  Type tonum1_t = Type::makeFun(Type::makeStr(true), Type::makeNum());

  Type text0_t = Type::makeLst({Type::makeStr(false)}, false, false);

  // map tonum {:text:}
  fails+= doApplied(map2_t, {tonum1_t, text0_t}, {
    "(a -> b) -> [a]* -> [b]*",
    "[Str*]* -> [Num]*",
    "[Num]",
  });

  Type zipwith3_t = Type::makeFun(
    Type::makeFun(
      Type::makeUnk("a_zipwith"),
      Type::makeFun(
        Type::makeUnk("b_zipwith"),
        Type::makeUnk("c_zipwith")
      )
    ),
    Type::makeFun(
      Type::makeLst({Type::makeUnk("a_zipwith")}, true, false),
      Type::makeFun(
        Type::makeLst({Type::makeUnk("b_zipwith")}, true, false),
        Type::makeLst({Type::makeUnk("c_zipwith")}, true, false)
      )
    )
  );

  Type repeat1_t = Type::makeFun(
    Type::makeUnk("a_repeat"),
    Type::makeLst({Type::makeUnk("a_repeat")}, true, false)
  );

  Type list0_repeat1_t = Type::makeLst({repeat1_t}, false, false);

  Type list0_list0_num_t = Type::makeLst({
    Type::makeLst({Type::makeNum()}, false, false)
  }, false, false);

  // zipwith map {repeat} {{1}}
  fails+= doApplied(zipwith3_t, {map2_t, list0_repeat1_t, list0_list0_num_t}, {
    "(a -> b -> c) -> [a]* -> [b]* -> [c]*",
    "[a -> b]* -> [[a]*]* -> [[b]*]*",
    "[[a]] -> [[[a]*]]", // YYY: but should be: "[[a]*] -> [[[a]*]*]"
    "[[[Num]*]*]*", // YYY: but should be: "[[[Num]*]]"
  });

  // a -> b
  Type idk1_t = Type::makeFun(
    Type::makeUnk("a_idk"),
    Type::makeUnk("b_idk")
  );
  fails+= doApplied(idk1_t, {text0_t}, {"a -> b", "b"});

  return fails;
}
