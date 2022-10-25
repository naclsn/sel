// place holder file
#ifndef SEL_PRELUDE_HPP
#define SEL_PRELUDE_HPP

#include <string>

#include "sel/utils.hpp"
#include "sel/engine.hpp"

namespace sel {

  /**
   * Seach for a value by name, return nullptr if not found.
   */
  Val* lookup_name(Env& env, std::string const& name);

  struct Abs1 : Fun {
    Abs1(Env& env)
      : Fun(env, Type(Ty::FUN,
          {.box_pair={
            new Type(Ty::NUM, {0}, 0),
            new Type(Ty::NUM, {0}, 0)
          }}, 0
        ))
    { TRACE(Abs1, ":: " << ty); }
    Val* operator()(Val* arg) override;
    void accept(Visitor& v) const override;
  };
  struct Abs0 : Num {
    Abs1* base;
    Num* arg;
    Abs0(Env& env, Abs1* base, Num* arg)
      : Num(env)
      , base(base)
      , arg(arg)
    { TRACE(Abs0, ":: " << ty); }
    double value() override;
    void accept(Visitor& v) const override;
  };

  struct Add2 : Fun {
    Add2(Env& env)
      : Fun(env, Type(Ty::FUN,
          {.box_pair={
            new Type(Ty::NUM, {0}, 0),
            new Type(Ty::FUN,
              {.box_pair={
                new Type(Ty::NUM, {0}, 0),
                new Type(Ty::NUM, {0}, 0)
              }}, 0
            )
          }}, 0
        ))
    { TRACE(Add2, ":: " << ty); }
    Val* operator()(Val* arg) override;
    void accept(Visitor& v) const override;
  };
  struct Add1 : Fun {
    Add2* base;
    Num* arg;
    Add1(Env& env, Add2* base, Num* arg)
      : Fun(env, Type(Ty::FUN,
          {.box_pair={
            new Type(Ty::NUM, {0}, 0),
            new Type(Ty::NUM, {0}, 0)
          }}, 0
        ))
      , base(base)
      , arg(arg)
    { TRACE(Add1, ":: " << ty); }
    Val* operator()(Val* arg) override;
    void accept(Visitor& v) const override;
  };
  struct Add0 : Num {
    Add1* base;
    Num* arg;
    Add0(Env& env, Add1* base, Num* arg)
      : Num(env)
      , base(base)
      , arg(arg)
    { TRACE(Add0, ":: " << ty); }
    double value() override;
    void accept(Visitor& v) const override;
  };

  // join:: Str -> [Str]* -> Str*
  struct Join2 : Fun {
    Join2(Env& env)
      : Fun(env, Type(Ty::FUN,
          {.box_pair={
            new Type(Ty::STR, {0}, TyFlag::IS_FIN),
            new Type(Ty::FUN,
              {.box_pair={
                new Type(Ty::LST,
                  {.box_has=
                    types1(new Type(Ty::STR, {0}, TyFlag::IS_FIN))
                  }, TyFlag::IS_INF
                ),
                new Type(Ty::STR, {0}, TyFlag::IS_INF)
              }}, 0
            )
          }}, 0
        ))
    { TRACE(Join2, ":: " << ty); }
    Val* operator()(Val* arg) override;
    void accept(Visitor& v) const override;
  };
  struct Join1 : Fun {
    Join2* base;
    Str* arg;
    Join1(Env& env, Join2* base, Str* arg)
      : Fun(env, Type(Ty::FUN,
          {.box_pair={
            new Type(Ty::LST,
              {.box_has=
                types1(new Type(Ty::STR, {0}, TyFlag::IS_FIN))
              }, TyFlag::IS_INF
            ),
            new Type(Ty::STR, {0}, TyFlag::IS_INF)
          }}, 0
        ))
      , base(base)
      , arg(arg)
    { TRACE(Join1, ":: " << ty); }
    Val* operator()(Val* arg) override;
    void accept(Visitor& v) const override;
  };
  struct Join0 : Str {
    Join1* base;
    Lst* arg;
    bool beginning;
    Join0(Env& env, Join1* base, Lst* arg)
      : Str(env, (TyFlag)(TyFlag::IS_INF & arg->type().flags))
      , base(base)
      , arg(arg)
      , beginning(true)
    { TRACE(Join0, ":: " << ty); }
    std::ostream& stream(std::ostream& out) override;
    bool end() const override;
    void rewind() override;
    std::ostream& entire(std::ostream& out) override;
    void accept(Visitor& v) const override;
  };

  // map:: (a -> b) -> [a]* -> [b]*
  struct Map2 : Fun {
    Map2(Env& env)
      : Fun(env, Type(Ty::FUN,
          {.box_pair={
            new Type(Ty::FUN,
              {.box_pair={
                new Type(Ty::UNK, {.name=new std::string("a")}, 0),
                new Type(Ty::UNK, {.name=new std::string("b")}, 0)
              }}, 0
            ),
            new Type(Ty::FUN,
              {.box_pair={
                new Type(Ty::LST,
                  {.box_has=
                    types1(new Type(Ty::UNK, {.name=new std::string("a")}, 0))
                  }, TyFlag::IS_INF
                ),
                new Type(Ty::LST,
                  {.box_has=
                    types1(new Type(Ty::UNK, {.name=new std::string("b")}, 0))
                  }, TyFlag::IS_INF
                )
              }}, 0
            )
          }}, 0
        ))
    { TRACE(Map2, ":: " << ty); }
    Val* operator()(Val* arg) override;
    void accept(Visitor& v) const override;
  };
  struct Map1 : Fun {
    Map2* base;
    Fun* arg;
    Map1(Env& env, Map2* base, Fun* arg)
      : Fun(env, Type(Ty::FUN,
          {.box_pair={
            new Type(Ty::LST,
              {.box_has=
                types1(new Type(arg->type().from()))
              }, TyFlag::IS_INF
            ),
            new Type(Ty::LST,
              {.box_has=
                types1(new Type(arg->type().to()))
              }, TyFlag::IS_INF
            )
          }}, 0
        ))
      , base(base)
      , arg(arg)
    { TRACE(Map1, ":: " << ty); }
    Val* operator()(Val* arg) override;
    void accept(Visitor& v) const override;
  };
  struct Map0 : Lst {
    Map1* base;
    Lst* arg;
    Map0(Env& env, Map1* base, Lst* arg)
      : Lst(env, Type(Ty::LST,
          {.box_has=
            types1(new Type(base->arg->type().to()))
          }, (TyFlag)(TyFlag::IS_INF & arg->type().flags)
        ))
    // Type(arg->type()), (TyFlag)arg->type().flags)
      , base(base)
      , arg(arg)
    { TRACE(Map0, ":: " << ty); }
    Val* operator*() override;
    Lst& operator++() override;
    bool end() const override;
    void rewind() override;
    size_t count() override;
    void accept(Visitor& v) const override;
  };

  // split :: Str -> Str* -> [Str]* -- NOTE: this assumes the delimiter will happend at some point, maybe it should be `[Str*]*` but this could make it annoying to work with
  struct Split2 : Fun {
    Split2(Env& env)
      : Fun(env, Type(Ty::FUN,
          {.box_pair={
            new Type(Ty::STR, {0}, TyFlag::IS_FIN),
            new Type(Ty::FUN,
              {.box_pair={
                new Type(Ty::STR, {0}, TyFlag::IS_INF),
                new Type(Ty::LST,
                  {.box_has=
                    types1(new Type(Ty::STR, {0}, TyFlag::IS_FIN))
                  }, TyFlag::IS_INF
                )
              }}, 0
            )
          }}, 0
        ))
    { TRACE(Split2, ":: " << ty); }
    Val* operator()(Val* arg) override;
    void accept(Visitor& v) const override;
  };
  struct Split1 : Fun {
    Split2* base;
    Str* arg;
    Split1(Env& env, Split2* base, Str* arg)
      : Fun(env, Type(Ty::FUN,
          {.box_pair={
            new Type(Ty::STR, {0}, TyFlag::IS_INF),
            new Type(Ty::LST,
              {.box_has=
                types1(new Type(Ty::STR, {0}, TyFlag::IS_FIN))
              }, TyFlag::IS_INF
            )
          }}, 0
        ))
      , base(base)
      , arg(arg)
    { TRACE(Split1, ":: " << ty); }
    Val* operator()(Val* arg) override;
    void accept(Visitor& v) const override;
  };
  struct Split0 : Lst {
    Split1* base;
    Str* arg;
    Split0(Env& env, Split1* base, Str* arg)
      : Lst(env, Type(Ty::LST,
          {.box_has=
            types1(new Type(Ty::STR, {0}, TyFlag::IS_FIN))
          }, (TyFlag)(TyFlag::IS_INF & arg->type().flags)
        ))
      , base(base)
      , arg(arg)
    { TRACE(Split0, ":: " << ty); }
    Val* operator*() override;
    Lst& operator++() override;
    bool end() const override;
    void rewind() override;
    size_t count() override;
    void accept(Visitor& v) const override;
  };

  struct Tonum1 : Fun {
    Tonum1(Env& env)
      : Fun(env, Type(Ty::FUN,
          {.box_pair={
            new Type(Ty::STR, {0}, TyFlag::IS_FIN),
            new Type(Ty::NUM, {0}, 0)
          }}, 0
        ))
    { TRACE(Tonum1, ":: " << ty); }
    Val* operator()(Val* arg) override;
    void accept(Visitor& v) const override;
  };
  struct Tonum0 : Num {
    Tonum1* base;
    Str* arg;
    Tonum0(Env& env, Tonum1* base, Str* arg)
      : Num(env)
      , base(base)
      , arg(arg)
    { TRACE(Tonum0, ":: " << ty); }
    double value() override;
    void accept(Visitor& v) const override;
  };

  struct Tostr1 : Fun {
    Tostr1(Env& env)
      : Fun(env, Type(Ty::FUN,
          {.box_pair={
            new Type(Ty::NUM, {0}, 0),
            new Type(Ty::STR, {0}, TyFlag::IS_FIN)
          }}, 0
        ))
    { TRACE(Tostr1, ":: " << ty); }
    Val* operator()(Val* arg) override;
    void accept(Visitor& v) const override;
  };
  struct Tostr0 : Str {
    Tostr1* base;
    Num* arg;
    bool read;
    Tostr0(Env& env, Tostr1* base, Num* arg)
      : Str(env, TyFlag::IS_FIN)
      , base(base)
      , arg(arg)
    { TRACE(Tostr0, ":: " << ty); }
    std::ostream& stream(std::ostream& out) override;
    bool end() const override;
    void rewind() override;
    std::ostream& entire(std::ostream& out) override;
    void accept(Visitor& v) const override;
  };

}

#endif // SEL_PRELUDE_HPP
