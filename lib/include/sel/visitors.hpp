#ifndef SEL_VISITORS_HPP
#define SEL_VISITORS_HPP

/**
 * Setup for "visitor pattern" over every derivatives of
 * `Val`. Also defines a few such as `ValRepr`. Note that
 * this pattern may be quite heavy and is not meant to be
 * implemented where performance matters.
 */

#include <iostream>
#include <ostream>
#include <string>
#include <vector>

#include "errors.hpp"
#include "forward.hpp"
#include "ll.hpp"
#include "types.hpp"

namespace sel {

  // DO NOT USE DIRECTLY, use `val.accept(visitor)`!
  template <typename Vi, typename Va>
  typename Vi::Ret visit(Vi& visitor, Va const& val) { return visitor.visit(val); }


  class VisRepr {
  public:
    typedef std::ostream& Ret;

    struct ReprCx {
      unsigned indents;
      bool top_level;
      bool single_line;
    };

    VisRepr(std::ostream& res, ReprCx cx={.top_level=true}): res(res), cx(cx) { }

  private:
    std::ostream& res;
    ReprCx cx;

    struct ReprField {
      char const* name;
      enum { DBL, STR, VAL } const data_ty;
      union { double const dbl; std::string const* str; Val const* val; } const;
      ReprField(char const* name, double dbl): name(name), data_ty(ReprField::DBL), dbl(dbl) { }
      ReprField(char const* name, std::string const* str): name(name), data_ty(ReprField::STR), str(str) { }
      ReprField(char const* name, Val const* val): name(name), data_ty(ReprField::VAL), val(val) { }
    };

    void reprHelper(Type const& type, char const* name, std::vector<ReprField> const fields);
    void visitCommon(Segment const& it, Type const&, char const* name, unsigned args, unsigned max_args);
    void visitCommon(Val const& it, Type const&, char const* name, unsigned args, unsigned max_args);

  public:
    template <typename T>
    Ret visit(T const& it) {
      visitCommon((typename std::conditional<!T::args, Val, Segment>::type&)it, it.type(), T::the::Base::Next::name, T::args, T::the::args);
      return res;
    }
    Ret visit(NumLiteral const& it);
    Ret visit(StrLiteral const& it);
    Ret visit(LstLiteral const& it);
    Ret visit(FunChain const& it);
    Ret visit(NumDefine const& it);
    Ret visit(StrDefine const& it);
    Ret visit(LstDefine const& it);
    Ret visit(FunDefine const& it);
    Ret visit(Input const& it);
    Ret visit(NumResult const& it);
    Ret visit(StrChunks const& it);
    Ret visit(LstMapCoerse const& it);
  };


  class VisHelp {
  public:
    typedef char const* Ret;

    template <typename T>
    Ret visit(T const& it) {
      return T::the::Base::Next::doc;
    }
    Ret visit(NumLiteral const& it) { throw TypeError("help(NumLiteral) does not make sense"); }
    Ret visit(StrLiteral const& it) { throw TypeError("help(StrLiteral) does not make sense"); }
    Ret visit(LstLiteral const& it) { throw TypeError("help(LstLiteral) does not make sense"); }
    Ret visit(FunChain const& it) { throw TypeError("help(FunChain) does not make sense"); }
    Ret visit(NumDefine const& it);
    Ret visit(StrDefine const& it);
    Ret visit(LstDefine const& it);
    Ret visit(FunDefine const& it);
    Ret visit(Input const& it) { throw TypeError("help(Input) does not make sense"); }
    Ret visit(NumResult const& it) { throw TypeError("help(NumResult) does not make sense"); }
    Ret visit(StrChunks const& it) { throw TypeError("help(StrChunks) does not make sense"); }
    Ret visit(LstMapCoerse const& it) { throw TypeError("help(LstMapCoerse) does not make sense"); }
  };


  class VisName {
  public:
    typedef char const* Ret;

    template <typename T>
    Ret visit(T const& it) {
      return T::the::Base::Next::name;
    }
    Ret visit(NumLiteral const& it) { throw TypeError("name(NumLiteral) does not make sense"); }
    Ret visit(StrLiteral const& it) { throw TypeError("name(StrLiteral) does not make sense"); }
    Ret visit(LstLiteral const& it) { throw TypeError("name(LstLiteral) does not make sense"); }
    Ret visit(FunChain const& it) { throw TypeError("name(FunChain) does not make sense"); }
    Ret visit(NumDefine const& it);
    Ret visit(StrDefine const& it);
    Ret visit(LstDefine const& it);
    Ret visit(FunDefine const& it);
    Ret visit(Input const& it) { throw TypeError("name(Input) does not make sense"); }
    Ret visit(NumResult const& it) { throw TypeError("name(NumResult) does not make sense"); }
    Ret visit(StrChunks const& it) { throw TypeError("name(StrChunks) does not make sense"); }
    Ret visit(LstMapCoerse const& it) { throw TypeError("name(LstMapCoerse) does not make sense"); }
  };


  class VisShow {
    std::ostream& res;

    void visitCommon(Segment const& it, char const* name, unsigned args, unsigned max_args);
    void visitCommon(Val const& it, char const* name, unsigned args, unsigned max_args);

  public:
    typedef std::ostream& Ret;

    VisShow(std::ostream& res): res(res) { }

    template <typename T>
    Ret visit(T const& it) {
      visitCommon((typename std::conditional<!T::args, Val, Segment>::type&)it, T::the::Base::Next::name, T::args, T::the::args);
      return res;
    }
    Ret visit(NumLiteral const& it);
    Ret visit(StrLiteral const& it);
    Ret visit(LstLiteral const& it);
    Ret visit(FunChain const& it);
    Ret visit(NumDefine const& it);
    Ret visit(StrDefine const& it);
    Ret visit(LstDefine const& it);
    Ret visit(FunDefine const& it);
    Ret visit(Input const& it);
    Ret visit(NumResult const& it);
    Ret visit(StrChunks const& it);
    Ret visit(LstMapCoerse const& it);
  };


  namespace visitors_ll {
    typedef ll::pack<VisRepr, VisHelp, VisName, VisShow> visitors;
  }

} // namespace sel

#endif // SEL_VISITORS_HPP
