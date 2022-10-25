#include "sel/visitors.hpp"

namespace sel {

#define fi_chr(__name, __chr) {.name=__name, .data_ty=ReprField::CHR, .data={.chr=__chr}}
#define fi_str(__name, __str) {.name=__name, .data_ty=ReprField::STR, .data={.str=__str}}
#define fi_val(__name, __val) {.name=__name, .data_ty=ReprField::VAL, .data={.val=__val}}

  void VisRepr::reprHelper(Type const& type, char const* name, std::initializer_list<ReprField> const fields) {
    std::vector<ReprField> tmp(fields);
    reprHelper(type, name, tmp);
  }
  void VisRepr::reprHelper(Type const& type, char const* name, std::vector<ReprField> const fields) {
    bool isln = 1 < fields.size() && !cx.single_line;

    std::string ln = " ";
    std::string ind = "";
    std::string lnind;
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
        case ReprField::CHR:
          res << " \"" << it.data.chr << "\"";
          break;

        case ReprField::STR:
          {
            res << " \"";
            std::string::size_type from = 0, to = it.data.str->find('"');
            while (std::string::npos != to) {
              res << it.data.str->substr(from, to) << "\\\"";
              from = to+1;
              to = it.data.str->find('"', from);
            }
            res << it.data.str->substr(from) << "\"";
          }
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
    auto nn = std::to_string(n);
    reprHelper(type, "NumLiteral", {
      fi_str("n", &nn),
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

  void VisRepr::visitStdin(Type const& type) {
    reprHelper(type, "Stdin", {});
  }

  void VisRepr::visitStdout(Type const& type) {
    reprHelper(type, "Stdout", {});
  }


  void VisRepr::visitAbs1(Type const& type) {
    reprHelper(type, "Abs1", {});
  }

  void VisRepr::visitAbs0(Type const& type, Val const* base, Val const* arg) {
    reprHelper(type, "Abs0", {
      fi_val("base", base),
      fi_val("arg", arg),
    });
  }

  void VisRepr::visitAdd2(Type const& type) {
    reprHelper(type, "Add2", {});
  }

  void VisRepr::visitAdd1(Type const& type, Val const* base, Val const* arg) {
    reprHelper(type, "Add1", {
      fi_val("base", base),
      fi_val("arg", arg),
    });
  }

  void VisRepr::visitAdd0(Type const& type, Val const* base, Val const* arg) {
    reprHelper(type, "Add0", {
      fi_val("base", base),
      fi_val("arg", arg),
    });
  }

  void VisRepr::visitJoin2(Type const& type) {
    reprHelper(type, "Join2", {});
  }

  void VisRepr::visitJoin1(Type const& type, Val const* base, Val const* arg) {
    reprHelper(type, "Join1", {
      fi_val("base", base),
      fi_val("arg", arg),
    });
  }

  void VisRepr::visitJoin0(Type const& type, Val const* base, Val const* arg) {
    reprHelper(type, "Join0", {
      fi_val("base", base),
      fi_val("arg", arg),
    });
  }

  void VisRepr::visitMap2(Type const& type) {
    reprHelper(type, "Map2", {});
  }

  void VisRepr::visitMap1(Type const& type, Val const* base, Val const* arg) {
    reprHelper(type, "Map1", {
      fi_val("base", base),
      fi_val("arg", arg),
    });
  }

  void VisRepr::visitMap0(Type const& type, Val const* base, Val const* arg) {
    reprHelper(type, "Map0", {
      fi_val("base", base),
      fi_val("arg", arg),
    });
  }

  void VisRepr::visitSplit2(Type const& type) {
    reprHelper(type, "Split2", {});
  }

  void VisRepr::visitSplit1(Type const& type, Val const* base, Val const* arg) {
    reprHelper(type, "Split1", {
      fi_val("base", base),
      fi_val("arg", arg),
    });
  }

  void VisRepr::visitSplit0(Type const& type, Val const* base, Val const* arg) {
    reprHelper(type, "Split0", {
      fi_val("base", base),
      fi_val("arg", arg),
    });
  }

  void VisRepr::visitTonum1(Type const& type) {
    reprHelper(type, "Tonum1", {});
  }

  void VisRepr::visitTonum0(Type const& type, Val const* base, Val const* arg) {
    reprHelper(type, "Tonum0", {
      fi_val("base", base),
      fi_val("arg", arg),
    });
  }

  void VisRepr::visitTostr1(Type const& type) {
    reprHelper(type, "Tostr1", {});
  }

  void VisRepr::visitTostr0(Type const& type, Val const* base, Val const* arg) {
    reprHelper(type, "Tostr0", {
      fi_val("base", base),
      fi_val("arg", arg),
    });
  }

} // namespace sel
