#include "sel/visitors.hpp"

namespace sel {

#define fi_dbl(__name, __dbl) {.name=__name, .data_ty=ReprField::DBL, .data={.dbl=__dbl}}
#define fi_str(__name, __str) {.name=__name, .data_ty=ReprField::STR, .data={.str=__str}}
#define fi_val(__name, __val) {.name=__name, .data_ty=ReprField::VAL, .data={.val=__val}}

  void _VisRepr<bins_ll::nil>::reprHelper(Type const& type, char const* name, std::vector<ReprField> const fields) {
    bool isln = 1 < fields.size() && !cx.single_line;

    std::string ln = " ";
    std::string ind = "";
    std::string lnind = " ";
    ind.reserve(3 * cx.indents + 1);
    for (unsigned k = 0; k < cx.indents; k++)
      ind.append("   ");
    if (isln) {
      ln = "\n";
      lnind = ln+ind;
    }

    // `iswrap` when the single field is a `Val` (so no increase indent)
    bool iswrap = 1 == fields.size()
      && ReprField::VAL == fields.begin()->data_ty;

    if (cx.top_level) res << ind;
    res << "<" << type << "> " << name << " {" << lnind;
    if (!iswrap) cx.indents++;

    for (auto& it : fields) {
      if (isln) res << "   ";
      res << it.name << "=";

      switch (it.data_ty) {
        case ReprField::DBL:
          res << " " << it.data.dbl;
          break;

        case ReprField::STR:
          res << " " << quoted(*it.data.str);
          break;

        case ReprField::VAL:
          if (it.data.val) {
            bool was_top = cx.top_level;
            cx.top_level = false;
            this->operator()(*it.data.val);
            cx.top_level = was_top;
          } else res << " -nil-";
          break;
      }

      res << lnind;
    }

    if (!iswrap) cx.indents--;
    res << "}";
    if (cx.top_level) res << "\n";
  }


  void VisRepr::visitNumLiteral(Type const& type, double n) {
    reprHelper(type, "NumLiteral", {
      fi_dbl("n", n),
    });
  }

  void VisRepr::visitStrLiteral(Type const& type, std::string const& s) {
    reprHelper(type, "StrLiteral", {
      fi_str("s", &s),
    });
  }

  void VisRepr::visitLstLiteral(Type const& type, std::vector<Val*> const& v) {
    size_t c = v.size();
    std::vector<char[16]> b(c);
    std::vector<ReprField> a;
    a.reserve(c);
    for (size_t k = 0; k < c; k++) {
      std::sprintf(b[k], "v[%zu]", k);
      a.push_back(fi_val(b[k], v[k]));
    }
    reprHelper(type, "LstLiteral", a);
  }

  void VisRepr::visitFunChain(Type const& type, std::vector<Fun*> const& f) {
    size_t c = f.size();
    std::vector<char[16]> b(c);
    std::vector<ReprField> a;
    a.reserve(c);
    for (size_t k = 0; k < c; k++) {
      std::sprintf(b[k], "f[%zu]", k);
      a.push_back(fi_val(b[k], (Val*)f[k]));
    }
    reprHelper(type, "FunChain", a);
  }

  void VisRepr::visitStrChunks(Type const& type, std::vector<std::string> const& vs) {
    size_t c = vs.size();
    std::vector<char[16]> b(c);
    std::vector<ReprField> a;
    a.reserve(c);
    for (size_t k = 0; k < c; k++) {
      std::sprintf(b[k], "v[%zu]", k);
      a.push_back(fi_str(b[k], &vs[k]));
    }
    reprHelper(type, "StrChunks", a);
  }

  void VisRepr::visitLstMapCoerse(Type const& type, Lst const& l) {
    reprHelper(type, "LstMapCoerse", {
      fi_val("l", &l),
    });
  }

  void VisRepr::visitInput(Type const& type) {
    reprHelper(type, "Input", {});
  }

  template <typename T>
  void _VisRepr<bins_ll::nil>::visitCommon(T const& it, std::false_type is_head) {
    std::string normalized // capitalizes first letter and append arity
      = (char)(T::the::Base::Next::name[0]+('A'-'a'))
      + ((T::the::Base::Next::name+1)
      + std::to_string(T::the::args-T::args));
    reprHelper(it.type(), normalized.c_str(), {
      fi_val("base", it.base),
      fi_val("arg", it.arg),
    });
  }

  template <typename T>
  void _VisRepr<bins_ll::nil>::visitCommon(T const& it, std::true_type is_head) {
    std::string normalized // capitalizes first letter and append arity
      = (char)(T::the::Base::Next::name[0]+('A'-'a'))
      + ((T::the::Base::Next::name+1)
      + std::to_string(T::the::args-T::args));
    reprHelper(it.type(), normalized.c_str(), {});
  }

} // namespace sel
