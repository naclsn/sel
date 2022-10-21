#include "sel/visitors.hpp"

namespace sel {

#define fi_chr(__name, __chr) {.name=__name, .data_ty=ReprField::CHR, .data={.chr=__chr}}
#define fi_str(__name, __str) {.name=__name, .data_ty=ReprField::STR, .data={.str=__str}}
#define fi_val(__name, __val) {.name=__name, .data_ty=ReprField::VAL, .data={.val=__val}}

  void ValRepr::reprHelper(Type const& type, char const* name, std::initializer_list<ReprField const> const fields) {
    bool isln = 1 < fields.size();

    std::string ln = " ";
    if (isln) {
      ln = "\n";
      ln.reserve(3 * cx.indents + 2);
      for (unsigned k = 0; k < cx.indents; k++)
        ln.append("   ");
    }

    // `iswrap` when the single field is a `Val` (so no increase indent)
    bool iswrap = 1 == fields.size()
      && ReprField::VAL == fields.begin()->data_ty;

    res << "<" << type << "> " << name << " {" << ln;
    if (!iswrap) cx.indents++;

    for (auto& it : fields) {
      if (isln) res << "   ";
      res << it.name << "=";

      switch (it.data_ty) {
        case ReprField::CHR:
          res << " \"" << it.data.chr << "\"";
          break;

        case ReprField::STR:
          res << " \"" << *it.data.str << "\"";
          break;

        case ReprField::VAL:
          if (it.data.val) {
            this->operator()(*it.data.val);
          } else res << " -nil-";
          break;
      }

      res << ln;
    }

    if (!iswrap) cx.indents--;
    res << "}";
  }

  void ValRepr::visitBidoof(Type const& type) {
    reprHelper(type, "Bidoof", {});
  }

  void ValRepr::visitNumLiteral(Type const& type, double n) {
    auto nn = std::to_string(n);
    reprHelper(type, "NumLiteral", {
      fi_str("n", &nn),
    });
  }

  void ValRepr::visitAbs1(Type const& type) {
    reprHelper(type, "Abs1", {});
  }

  void ValRepr::visitAbs0(Type const& type, Val const* base, Val const* arg) {
    reprHelper(type, "Abs0", {
      fi_val("base", base),
      fi_val("arg", arg),
    });
  }

  void ValRepr::visitAdd2(Type const& type) {
    reprHelper(type, "Add2", {});
  }

  void ValRepr::visitAdd1(Type const& type, Val const* base, Val const* arg) {
    reprHelper(type, "Add1", {
      fi_val("base", base),
      fi_val("arg", arg),
    });
  }

  void ValRepr::visitAdd0(Type const& type, Val const* base, Val const* arg) {
    reprHelper(type, "Add0", {
      fi_val("base", base),
      fi_val("arg", arg),
    });
  }

} // namespace sel
