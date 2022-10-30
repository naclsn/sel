#include "sel/builtins.hpp"
#include "sel/visitors.hpp"

namespace sel {

  Val* lookup_name(std::string const& name) {
    // if ("abs"   == name) return new Abs1();
    if ("add"   == name) return new bin::Add::Head();
    // if ("flip"  == name) return new Flip2();
    if ("idk"   == name) return new bin::Idk::Head();
    // if ("join"  == name) return new Join2();
    // if ("map"   == name) return new Map2();
    // if ("split" == name) return new Split2();
    if ("sub"   == name) return new bin::Sub::Head();
    // if ("tonum" == name) return new Tonum1();
    // if ("tostr" == name) return new Tostr1();
    return nullptr;
  }

  template <typename NextT, Ty From, Ty... To>
  void bin_val_helpers::bin_val<NextT, From, To...>::accept(Visitor& v) const {
    v.visit(*this); // visitBody
  }

  template <typename NextT, Ty LastFrom, Ty LastTo>
  void bin_val_helpers::bin_val<NextT, LastFrom, LastTo>::the::accept(Visitor& v) const {
    v.visit(*(typename Base::Next*)this); // visitTail
  };

  template <typename NextT, Ty LastFrom, Ty LastTo>
  void bin_val_helpers::bin_val<NextT, LastFrom, LastTo>::accept(Visitor& v) const {
    v.visit(*this); // visitHead
  }

  // ZZZ
  namespace bin_val_helpers {

    struct Unk { };
    template <char c> struct name { };

    template <typename T> struct make_type;

    template <char c>
    struct make_type<Unk(name<c>)> {
      inline static Type the() { return Type(Ty::UNK, {.name=new std::string(1, c)}, 0); }
    };

    template <>
    struct make_type<Num> {
      inline static Type the() { return Type(Ty::NUM, {0}, 0); }
    };

    template <>
    struct make_type<Str> {
      inline static Type the(TyFlag is_inf) { return Type(Ty::STR, {0}, is_inf); }
    };
    template <>
    struct make_type<Str(std::false_type)> {
      inline static Type the() { return Type(Ty::STR, {0}, TyFlag::IS_FIN); }
    };
    template <>
    struct make_type<Str(std::true_type)> {
      inline static Type the() { return Type(Ty::STR, {0}, TyFlag::IS_INF); }
    };

    template <>
    struct make_type<Lst> {
      inline static Type the(Type* t, TyFlag is_inf) {
        return Type(Ty::LST,
          {.box_has=types1(t)}, is_inf // is_tpl
        );
      }
    };

    using namespace std;
    void bidoof() {
      Type t_num = make_type<Num>::the();
      Type t_str = make_type<Str(std::false_type)>::the();
      Type t_str_i = make_type<Str(std::true_type)>::the();
      Type t_unk_a = make_type<Unk(name<'a'>)>::the();
      cerr << "t_num: " << t_num << "\n";
      cerr << "t_str: " << t_str << "\n";
      cerr << "t_str_i: " << t_str_i << "\n";
      cerr << "t_unk_a: " << t_unk_a << "\n";
    }

  } // namespace bin_val_helpers

} // namespace sel
