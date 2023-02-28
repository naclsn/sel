#include "sel/builtins.hpp"
#include "sel/parser.hpp"
#include "sel/utils.hpp"
#include "sel/visitors.hpp"

namespace sel {

  void VisRepr::reprHelper(Type const& type, char const* name, std::vector<ReprField> const fields) {
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
          res << " " << it.dbl;
          break;

        case ReprField::STR:
          res << " " << quoted(*it.str);
          break;

        case ReprField::VAL:
          if (it.val) {
            bool was_top = cx.top_level;
            cx.top_level = false;
            it.val->accept(*this);
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


  VisRepr::Ret VisRepr::visit(NumLiteral const& it) {
    reprHelper(it.type(), "NumLiteral", {
      {"n", it.underlying()},
    });
    return res;
  }

  VisRepr::Ret VisRepr::visit(StrLiteral const& it) {
    reprHelper(it.type(), "StrLiteral", {
      {"s", &it.underlying()},
    });
    return res;
  }

  VisRepr::Ret VisRepr::visit(LstLiteral const& it) {
    auto const& v = it.underlying();
    size_t c = v.size();
    std::vector<char[16]> b(c);
    std::vector<ReprField> a;
    a.reserve(c);
    for (size_t k = 0; k < c; k++) {
      std::sprintf(b[k], "v[%zu]", k);
      a.push_back({b[k], v[k]});
    }
    reprHelper(it.type(), "LstLiteral", a);
    return res;
  }

  VisRepr::Ret VisRepr::visit(FunChain const& it) {
    auto const& f = it.underlying();
    size_t c = f.size();
    std::vector<char[16]> b(c);
    std::vector<ReprField> a;
    a.reserve(c);
    for (size_t k = 0; k < c; k++) {
      std::sprintf(b[k], "f[%zu]", k);
      a.push_back({b[k], (Val*)f[k]});
    }
    reprHelper(it.type(), "FunChain", a);
    return res;
  }

  VisRepr::Ret VisRepr::visit(NumDefine const& it) {
    reprHelper(it.type(), "NumDefine", {
      {"v", &it.underlying()},
      {"name", &it.getname()},
      {"doc", &it.getdoc()},
    });
    return res;
  }

  VisRepr::Ret VisRepr::visit(StrDefine const& it) {
    reprHelper(it.type(), "StrDefine", {
      {"v", &it.underlying()},
      {"name", &it.getname()},
      {"doc", &it.getdoc()},
    });
    return res;
  }

  VisRepr::Ret VisRepr::visit(LstDefine const& it) {
    reprHelper(it.type(), "LstDefine", {
      {"v", &it.underlying()},
      {"name", &it.getname()},
      {"doc", &it.getdoc()},
    });
    return res;
  }

  VisRepr::Ret VisRepr::visit(FunDefine const& it) {
    reprHelper(it.type(), "FunDefine", {
      {"v", &it.underlying()},
      {"name", &it.getname()},
      {"doc", &it.getdoc()},
    });
    return res;
  }

  VisRepr::Ret VisRepr::visit(Input const& it) {
    reprHelper(it.type(), "Input", {});
    return res;
  }

  VisRepr::Ret VisRepr::visit(StrChunks const& it) {
    auto const& vs = it.chunks();
    size_t c = vs.size();
    std::vector<char[16]> b(c);
    std::vector<ReprField> a;
    a.reserve(c);
    for (size_t k = 0; k < c; k++) {
      std::sprintf(b[k], "v[%zu]", k);
      a.push_back({b[k], &vs[k]});
    }
    reprHelper(it.type(), "StrChunks", a);
    return res;
  }

  VisRepr::Ret VisRepr::visit(LstMapCoerse const& it) {
    reprHelper(it.type(), "LstMapCoerse", {
      {"l", &it.source()},
    });
    return res;
  }

  void VisRepr::visitCommon(Segment const& it, Type const& type, char const* name, unsigned args, unsigned max_args) {
    std::string normalized // capitalizes first letter and append arity
      = (char)(name[0]+('A'-'a'))
      + ((name+1)
      + std::to_string(max_args-args));
    reprHelper(type, normalized.c_str(), {
      {"base", &it.base()},
      {"arg", &it.arg()},
    });
  }

  void VisRepr::visitCommon(Val const& it, Type const& type, char const* name, unsigned args, unsigned max_args) {
    std::string normalized // capitalizes first letter and append arity
      = (char)(name[0]+('A'-'a'))
      + ((name+1)
      + std::to_string(max_args-args));
    reprHelper(type, normalized.c_str(), {});
  }

} // namespace sel
