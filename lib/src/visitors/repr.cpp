#include "sel/visitors.hpp"
#include "sel/engine.hpp"

namespace sel {

#define fi_chr(__name, __chr) {.name=__name, .data_ty=ReprField::CHR, .data={.chr=__chr}}
#define fi_str(__name, __str) {.name=__name, .data_ty=ReprField::STR, .data={.str=__str}}
#define fi_val(__name, __val) {.name=__name, .data_ty=ReprField::VAL, .data={.val=__val}}

  void ValRepr::reprHelper(Type const& type, char const* name, std::initializer_list<ReprField const> const fields) {
    bool isln = 1 < fields.size();
    std::string ln = isln
      ? "\n" + std::string("   ", cx.indents)
      : " ";

    // `iswrap` when the single field is a `Val` (so no increase indent)
    bool iswrap = 1 == fields.size()
      && ReprField::VAL == fields.begin()->data_ty;

    res << "(" << type << ") " << name << " {" << ln;
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
            it.data.val->accept(*this);
          } else res << " -nil-";
          break;
      }

      res << ln;
    }

    if (!iswrap) cx.indents--;
    res << "}";
  }

  void ValRepr::visitBidoof(Type const& type, char const* some, Val const* other) {
    if (Ty::NUM == type.base)
      reprHelper(type, "Bidoof", {
        fi_val("other", other),
      });
    else
      reprHelper(type, "Bidoof", {
        fi_chr("some", some),
        fi_val("other", other),
      });
  }

} // namespace sel
