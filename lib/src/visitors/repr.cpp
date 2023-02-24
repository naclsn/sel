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


  void VisRepr::visitNumLiteral(Type const& type, double n) {
    reprHelper(type, "NumLiteral", {
      ReprField("n", n),
    });
  }

  void VisRepr::visitStrLiteral(Type const& type, std::string const& s) {
    reprHelper(type, "StrLiteral", {
      ReprField("s", &s),
    });
  }

  void VisRepr::visitLstLiteral(Type const& type, std::vector<Val*> const& v) {
    size_t c = v.size();
    std::vector<char[16]> b(c);
    std::vector<ReprField> a;
    a.reserve(c);
    for (size_t k = 0; k < c; k++) {
      std::sprintf(b[k], "v[%zu]", k);
      a.push_back(ReprField(b[k], v[k]));
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
      a.push_back(ReprField(b[k], (Val*)f[k]));
    }
    reprHelper(type, "FunChain", a);
  }

  void VisRepr::visitInput(Type const& type) {
    reprHelper(type, "Input", {});
  }

  void VisRepr::visitStrChunks(Type const& type, std::vector<std::string> const& vs) {
    size_t c = vs.size();
    std::vector<char[16]> b(c);
    std::vector<ReprField> a;
    a.reserve(c);
    for (size_t k = 0; k < c; k++) {
      std::sprintf(b[k], "v[%zu]", k);
      a.push_back(ReprField(b[k], &vs[k]));
    }
    reprHelper(type, "StrChunks", a);
  }

  void VisRepr::visitLstMapCoerse(Type const& type, Lst const& l) {
    reprHelper(type, "LstMapCoerse", {
      ReprField("l", &l),
    });
  }

  void VisRepr::visitCommon(Segment const& it, Type const& type, char const* name, unsigned args, unsigned max_args) {
    std::string normalized // capitalizes first letter and append arity
      = (char)(name[0]+('A'-'a'))
      + ((name+1)
      + std::to_string(max_args-args));
    reprHelper(type, normalized.c_str(), {
      ReprField("base", &it.base()),
      ReprField("arg", &it.arg()),
    });
  }

  void VisRepr::visitCommon(Val const& it, Type const& type, char const* name, unsigned args, unsigned max_args) {
    std::string normalized // capitalizes first letter and append arity
      = (char)(name[0]+('A'-'a'))
      + ((name+1)
      + std::to_string(max_args-args));
    reprHelper(type, normalized.c_str(), {});
  }

  VisRepr::Ret VisRepr::visit(NumLiteral const& it) {
    visitNumLiteral(it.type(), it.underlying());
    return res;
  }
  VisRepr::Ret VisRepr::visit(StrLiteral const& it) {
    visitStrLiteral(it.type(), it.underlying());
    return res;
  }
  VisRepr::Ret VisRepr::visit(LstLiteral const& it) {
    visitLstLiteral(it.type(), it.underlying());
    return res;
  }
  VisRepr::Ret VisRepr::visit(FunChain const& it) {
    visitFunChain(it.type(), it.underlying());
    return res;
  }

  VisRepr::Ret VisRepr::visit(Input const& it) {
    visitInput(it.type());
    return res;
  }

  VisRepr::Ret VisRepr::visit(StrChunks const& it) {
    visitStrChunks(it.type(), it.chunks());
    return res;
  }
  VisRepr::Ret VisRepr::visit(LstMapCoerse const& it) {
    visitLstMapCoerse(it.type(), it.source());
    return res;
  }

} // namespace sel
