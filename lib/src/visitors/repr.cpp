#include "sel/visitors.hpp"
#include "sel/engine.hpp"

namespace sel {

#define fi_chr(__name, __chr) {.name=__name, .data_ty=ReprField::CHR, .data={.chr=__chr}}
#define fi_str(__name, __str) {.name=__name, .data_ty=ReprField::STR, .data={.str=__str}}
#define fi_val(__name, __val) {.name=__name, .data_ty=ReprField::VAL, .data={.val=__val}}

  void ValRepr::reprHelper(char const* name, std::initializer_list<ReprField const> const fields) {
    char const* ln = 1 < fields.size()
      ? "\n"
      : " ";
    res << name << " {" << ln;

    for (auto& it : fields) {
      for (unsigned k = 0; k < cx.indents+1; k++)
        res << "\t";
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
            res << "(" << it.data.val->type() << ") ";
            cx.indents++;
            it.data.val->accept(*this);
            cx.indents--;
            // res << "it.data.val->accept(*this)";
          } else {
            res << " -nil-";
          }
          break;
      }

      res << ln;
    }

    if (1 < fields.size())
      for (unsigned k = 0; k < cx.indents; k++)
        res << "\t";

    res << "}\n";
  }

  void ValRepr::visitCrap(char const* some, Val const* other) {
    reprHelper("Crap", {
      fi_chr("some", some),
      fi_val("other", other),
    });
  }

} // namespace sel
