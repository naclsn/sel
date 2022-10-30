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

  void VisRepr::visitInput(Type const& type) {
    reprHelper(type, "Input", {});
  }

  void VisRepr::visitOutput(Type const& type) {
    reprHelper(type, "Output", {});
  }

  template <typename T>
  void VisRepr::visitCommon(T const& it, std::false_type is_head) {
    constexpr unsigned arity = T::the::args - T::args;
    reprHelper(it.type(), (std::string(T::the::Base::Next::name) + std::to_string(arity)).c_str(), {
      fi_val("base", it.base),
      fi_val("arg", it.arg),
    });
  }

  template <typename T>
  void VisRepr::visitCommon(T const& it, std::true_type is_head) {
    constexpr unsigned arity = T::the::args - T::args;
    reprHelper(it.type(), (std::string(T::the::Base::Next::name) + std::to_string(arity)).c_str(), {});
  }

#if 0
  void VisRepr::visit(bin::Add const& it) {
    visitCommon(it, std::conditional<!bin::Add::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bin::Add::Base const& it) {
    visitCommon(it, std::conditional<!bin::Add::Base::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bin::Add::Base::Base const& it) {
    visitCommon(it, std::conditional<!bin::Add::Base::Base::args, std::true_type, std::false_type>::type{});
  }

  void VisRepr::visit(bin::Sub const& it) {
    visitCommon(it, std::conditional<!bin::Sub::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bin::Sub::Base const& it) {
    visitCommon(it, std::conditional<!bin::Sub::Base::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bin::Sub::Base::Base const& it) {
    visitCommon(it, std::conditional<!bin::Sub::Base::Base::args, std::true_type, std::false_type>::type{});
  }
#endif

} // namespace sel
