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
#include <sstream>
#include <string>
#include <vector>
#include <functional>
#include <stack>
#include <list>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_os_ostream.h>
namespace tll { template <typename Ty> struct let; }

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

    void visitNumLiteral(Type const& type, double n);
    void visitStrLiteral(Type const& type, std::string const& s);
    void visitLstLiteral(Type const& type, std::vector<Val*> const& v);
    void visitFunChain(Type const& type, std::vector<Fun*> const& f);
    void visitInput(Type const& type);
    void visitStrChunks(Type const& type, std::vector<std::string> const& vs);
    void visitLstMapCoerse(Type const& type, Lst const& l);

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
    Ret visit(Input const& it);
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
    Ret visit(StrChunks const& it) { throw TypeError("help(StrChunks) does not make sense"); }
    Ret visit(LstMapCoerse const& it) { throw TypeError("help(LstMapCoerse) does not make sense"); }
    Ret visit(FunChain const& it) { throw TypeError("help(FunChain) does not make sense"); }
    Ret visit(Input const& it) { throw TypeError("help(Input) does not make sense"); }
  };


  class VisCodegen : public Visitor {
    llvm::LLVMContext context;
    llvm::IRBuilder<> builder;
    llvm::Module module;

    indented& log;

    // things that can come out of SyGen, passed to the injection `also(brk, cont, <>)`
    struct Generated;

    // structure for 'segments' of a bin
    struct Segment; // virtual void operator(inject_clo_type also) = 0;
    // the `visit(::Head)` will be the actual implementation
    struct Head; // : Segment
    // excluding the Head, each part that are `{arg, base}` closures
    struct NotHead; // : Segment

    //{{{ some codegen utils

    // note that the `body` function gets the bytes pointed at,
    // not the iteration variable (ie a chr, not chr* nor len)
    void makeBufferLoop(std::string const name
      , tll::let<char const*> ptr
      , tll::let<int> len
      , std::function<void(llvm::BasicBlock* brk, llvm::BasicBlock* cont, tll::let<char> at)> body
      );

    void makeStream(std::string const name
      , std::function<void(llvm::BasicBlock* brk, llvm::BasicBlock* cont)> chk
      , std::function<Generated()> itr
      , std::function<void(llvm::BasicBlock* brk, llvm::BasicBlock* cont, Generated it)> also
      );

    //}}} some codegen utils

    //{{{ symbol shenanigan

    /// the injection (sometimes `also`) must branch to exit or iter
    /// in the case of a SyNum, brk and cont are both the same
    typedef std::function
      < void
        (llvm::BasicBlock* brk, llvm::BasicBlock* cont, Generated it)
      > inject_clo_type;
    typedef std::function<void(inject_clo_type)> clo_type;

    struct Symbol {
      std::string name;
      clo_type doobidoo;

      Symbol(std::string const name, clo_type doobidoo)
        : name(name)
        , doobidoo(doobidoo)
      { }
      virtual ~Symbol() { }

      virtual void make(inject_clo_type also) const { doobidoo(also); }
    };

    std::stack<Symbol, std::list<Symbol>> systack;
    void place(std::string const, clo_type doobidoo);
    Symbol const take();

    void invoke(Val const&);

    //}}} symbol shenanigan

  public:
    char const* funname;

    VisCodegen(char const* file_name, char const* module_name, char const* function_name, App& app);
    ~VisCodegen() { delete &log; }

    // generate a `i32 main()` that calls the generated function
    void makeMain();

    void print(std::ostream& out) const {
      llvm::raw_os_ostream o(out);
      module.print(o, nullptr);
    }
    void compile(char const* outfile, bool link);

    void visitNumLiteral(Type const& type, double n) override;
    void visitStrLiteral(Type const& type, std::string const& s) override;
    void visitLstLiteral(Type const& type, std::vector<Val*> const& v) override;
    void visitStrChunks(Type const& type, std::vector<std::string> const& vs) override;
    void visitFunChain(Type const& type, std::vector<Fun*> const& f) override;
    void visitInput(Type const& type) override;

    void visit(bins::abs_::Base const&) override;
    void visit(bins::abs_ const&) override;
    void visit(bins::add_::Base::Base const&) override;
    void visit(bins::add_::Base const&) override;
    void visit(bins::add_ const&) override;
    void visit(bins::bytes_::Base const&) override;
    void visit(bins::bytes_ const&) override;
    void visit(bins::const_::Base const&) override;
    void visit(bins::const_ const&) override;
    void visit(bins::id_ const&) override;
    void visit(bins::map_::Base::Base const&) override;
    void visit(bins::map_::Base const&) override;
    void visit(bins::map_ const&) override;
    void visit(bins::sub_::Base::Base const&) override;
    void visit(bins::sub_::Base const&) override;
    void visit(bins::sub_ const&) override;
    void visit(bins::tonum_::Base const&) override;
    void visit(bins::tonum_ const&) override;
    void visit(bins::tostr_::Base const&) override;
    void visit(bins::tostr_ const&) override;
    void visit(bins::unbytes_::Base const&) override;
    void visit(bins::unbytes_ const&) override;
  };

  namespace visitors_ll {
    typedef ll::pack<VisRepr, VisHelp, VisCodegen> visitors;
  }

} // namespace sel

#endif // SEL_VISITORS_HPP
