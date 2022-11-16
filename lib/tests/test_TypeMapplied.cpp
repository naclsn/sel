#include "common.hpp"

void pushAssert(vector<Type>& all, Type const push, string const expect) {
  all.push_back(push);

  ostringstream oss;
  oss << all[all.size()-1];

  cout << "---\t" << all[all.size()-1] << '\n';
  assert_cmp(expect, oss.str());
}

void doApplied(Type fun, vector<Type> args, vector<string> reprx) {
  vector<Type> all;

  pushAssert(all, fun, reprx[0]);

  for (size_t k = 0; k < args.size(); k++) {
    cout << all[all.size()-1].from() << "  <-  " << args[k] << '\n';

    pushAssert(all, all[all.size()-1].applied(args[k]), reprx[k+1]);
  }
}

TEST(TypeMapplied) {
  Type map2_t = Type(Ty::FUN,
    {.box_pair={
      new Type(Ty::FUN,
        {.box_pair={
          new Type(Ty::UNK, {.name=new string("a_map")}, 0),
          new Type(Ty::UNK, {.name=new string("b_map")}, 0)
        }}, 0
      ),
      new Type(Ty::FUN,
        {.box_pair={
          new Type(Ty::LST,
            {.box_has=new vector<Type*>({
              new Type(Ty::UNK, {.name=new string("a_map")}, 0)
            })}, TyFlag::IS_INF
          ),
          new Type(Ty::LST,
            {.box_has=new vector<Type*>({
              new Type(Ty::UNK, {.name=new string("b_map")}, 0)
            })}, TyFlag::IS_INF
          )
        }}, 0
      )
    }}, 0
  );

  Type tonum1_t = Type(Ty::FUN,
    {.box_pair={
      new Type(Ty::STR, {0}, TyFlag::IS_FIN),
      new Type(Ty::NUM, {0}, 0)
    }}, 0
  );

  Type text0_t = Type(Ty::LST,
    {.box_has=new vector<Type*>({
      new Type(Ty::STR, {0}, TyFlag::IS_FIN)
    })}, TyFlag::IS_FIN
  );

  // map tonum :text:
  doApplied(map2_t, {tonum1_t, text0_t}, {
    "(a -> b) -> [a]* -> [b]*",
    "[Str]* -> [Num]*",
    "[Num]",
  });

  Type zipwith3_t = Type(Ty::FUN,
    {.box_pair={
      new Type(Ty::FUN,
        {.box_pair={
          new Type(Ty::UNK, {.name=new string("a_zipwith")}, 0),
          new Type(Ty::FUN,
            {.box_pair={
              new Type(Ty::UNK, {.name=new string("b_zipwith")}, 0),
              new Type(Ty::UNK, {.name=new string("c_zipwith")}, 0)
            }}, 0
          )
        }}, 0
      ),
      new Type(Ty::FUN,
        {.box_pair={
          new Type(Ty::LST,
            {.box_has=new vector<Type*>({
              new Type(Ty::UNK, {.name=new string("a_zipwith")}, 0)
            })}, TyFlag::IS_INF
          ),
          new Type(Ty::FUN,
            {.box_pair={
              new Type(Ty::LST,
                {.box_has=new vector<Type*>({
                  new Type(Ty::UNK, {.name=new string("b_zipwith")}, 0)
                })}, TyFlag::IS_INF
              ),
              new Type(Ty::LST,
                {.box_has=new vector<Type*>({
                  new Type(Ty::UNK, {.name=new string("c_zipwith")}, 0)
                })}, TyFlag::IS_INF
              )
            }}, 0
          )
        }}, 0
      )
    }}, 0
  );

  Type repeat1_t = Type(Ty::FUN,
    {.box_pair={
      new Type(Ty::UNK, {.name=new string("a_repeat")}, 0),
      new Type(Ty::LST,
        {.box_has=new vector<Type*>({
          new Type(Ty::UNK, {.name=new string("a_repeat")}, 0)
        })}, TyFlag::IS_INF
      )
    }}, 0
  );

  Type list0_repeat1_t = Type(Ty::LST,
    {.box_has=new vector<Type*>({
      new Type(repeat1_t)
    })}, TyFlag::IS_FIN
  );

  Type list0_list0_num_t = Type(Ty::LST,
    {.box_has=new vector<Type*>({
      new Type(Ty::LST,
        {.box_has=new vector<Type*>({
          new Type(Ty::NUM, {0}, 0)
        })}, TyFlag::IS_FIN
      )
    })}, TyFlag::IS_FIN
  );

  // zipwith map {repeat} {{1}}
  doApplied(zipwith3_t, {map2_t, list0_repeat1_t, list0_list0_num_t}, {
    "(a -> b -> c) -> [a]* -> [b]* -> [c]*",
    "[a -> b]* -> [[a]*]* -> [[b]*]*",
    "[[a]] -> [[[a]*]]", // YYY: but should be: "[[a]*] -> [[[a]*]*]"
    "[[[Num]*]*]*", // YYY: but should be: "[[[Num]*]]"
  });

  // a -> b
  Type idk1_t = Type(Ty::FUN,
    {.box_pair={
      new Type(Ty::UNK, {.name=new string("a_idk")}, 0),
      new Type(Ty::UNK, {.name=new string("b_idk")}, 0)
    }}, 0
  );
  doApplied(idk1_t, {text0_t}, {"a -> b", "b"});
}
