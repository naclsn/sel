#include "sel/visitors.hpp"

namespace sel {

#define fi_dbl(__name, __dbl) {.name=__name, .data_ty=ReprField::DBL, .data={.dbl=__dbl}}
#define fi_str(__name, __str) {.name=__name, .data_ty=ReprField::STR, .data={.str=__str}}
#define fi_val(__name, __val) {.name=__name, .data_ty=ReprField::VAL, .data={.val=__val}}

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
          res << " " << it.data.dbl;
          break;

        case ReprField::STR:
          {
            res << " \"";
            std::string::size_type from = 0, to = it.data.str->find_first_of({'\t', '\n', '\r', '"'});
            while (std::string::npos != to) {
              res << it.data.str->substr(from, to-from) << '\\';
              switch (it.data.str->at(to)) {
                case '\t': res << 't'; break;
                case '\n': res << 'n'; break;
                case '\r': res << 'r'; break;
                case '"':  res << '"'; break;
              }
              from = to+1;
              to = it.data.str->find_first_of({'\t', '\n', '\r', '"'}, from);
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

  void VisRepr::visitInput(Type const& type) {
    reprHelper(type, "Input", {});
  }

  void VisRepr::visitOutput(Type const& type) {
    reprHelper(type, "Output", {});
  }

  template <typename T>
  void VisRepr::visitCommon(T const& it, std::false_type is_head) {
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
  void VisRepr::visitCommon(T const& it, std::true_type is_head) {
    std::string normalized // capitalizes first letter and append arity
      = (char)(T::the::Base::Next::name[0]+('A'-'a'))
      + ((T::the::Base::Next::name+1)
      + std::to_string(T::the::args-T::args));
    reprHelper(it.type(), normalized.c_str(), {});
  }

  void VisRepr::visit(bins::abs_ const& it) {
    visitCommon(it, std::conditional<!bins::abs_::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::abs_::Base const& it) {
    visitCommon(it, std::conditional<!bins::abs_::Base::args, std::true_type, std::false_type>::type{});
  }

  void VisRepr::visit(bins::add_ const& it) {
    visitCommon(it, std::conditional<!bins::add_::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::add_::Base const& it) {
    visitCommon(it, std::conditional<!bins::add_::Base::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::add_::Base::Base const& it) {
    visitCommon(it, std::conditional<!bins::add_::Base::Base::args, std::true_type, std::false_type>::type{});
  }

  void VisRepr::visit(bins::const_ const& it) {
    // YYY: `_base`, because `it` itself does not have `arg` yet and its `base` is an awkward proxy
    visitCommon(it._base, std::conditional<!bins::const_::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::const_::Base const& it) {
    visitCommon(it, std::conditional<!bins::const_::Base::args, std::true_type, std::false_type>::type{});
  }

  void VisRepr::visit(bins::drop_ const& it) {
    visitCommon(it, std::conditional<!bins::drop_::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::drop_::Base const& it) {
    visitCommon(it, std::conditional<!bins::drop_::Base::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::drop_::Base::Base const& it) {
    visitCommon(it, std::conditional<!bins::drop_::Base::Base::args, std::true_type, std::false_type>::type{});
  }

  void VisRepr::visit(bins::dropwhile_ const& it) {
    visitCommon(it, std::conditional<!bins::dropwhile_::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::dropwhile_::Base const& it) {
    visitCommon(it, std::conditional<!bins::dropwhile_::Base::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::dropwhile_::Base::Base const& it) {
    visitCommon(it, std::conditional<!bins::dropwhile_::Base::Base::args, std::true_type, std::false_type>::type{});
  }

  void VisRepr::visit(bins::filter_ const& it) {
    visitCommon(it, std::conditional<!bins::filter_::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::filter_::Base const& it) {
    visitCommon(it, std::conditional<!bins::filter_::Base::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::filter_::Base::Base const& it) {
    visitCommon(it, std::conditional<!bins::filter_::Base::Base::args, std::true_type, std::false_type>::type{});
  }

  void VisRepr::visit(bins::flip_ const& it) {
    // YYY: `_base`, because `it` itself does not have `arg` yet and its `base` is an awkward proxy
    visitCommon(it._base, std::conditional<!bins::flip_::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::flip_::Base const& it) {
    visitCommon(it, std::conditional<!bins::flip_::Base::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::flip_::Base::Base const& it) {
    visitCommon(it, std::conditional<!bins::flip_::Base::Base::args, std::true_type, std::false_type>::type{});
  }

  void VisRepr::visit(bins::id_ const& it) {
    // YYY: NOT `_base`, because this time its not a function but a value (well its value is a function, but.. if that makes sense..?)
    //      it uses the specialization of the template that says 'visitOne2'
    //      tbh, it would be better to rather not use `_base` anywhere (see eg. const, flip, ...)
    visitCommon(it, std::conditional<!bins::id_::args, std::true_type, std::false_type>::type{});
  }

  void VisRepr::visit(bins::if_ const& it) {
    // YYY: `_base`, because `it` itself does not have `arg` yet and its `base` is an awkward proxy
    visitCommon(it._base, std::conditional<!bins::if_::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::if_::Base const& it) {
    visitCommon(it, std::conditional<!bins::if_::Base::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::if_::Base::Base const& it) {
    visitCommon(it, std::conditional<!bins::if_::Base::Base::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::if_::Base::Base::Base const& it) {
    visitCommon(it, std::conditional<!bins::if_::Base::Base::Base::args, std::true_type, std::false_type>::type{});
  }

  void VisRepr::visit(bins::iterate_ const& it) {
    visitCommon(it, std::conditional<!bins::iterate_::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::iterate_::Base const& it) {
    visitCommon(it, std::conditional<!bins::iterate_::Base::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::iterate_::Base::Base const& it) {
    visitCommon(it, std::conditional<!bins::iterate_::Base::Base::args, std::true_type, std::false_type>::type{});
  }

  void VisRepr::visit(bins::join_ const& it) {
    visitCommon(it, std::conditional<!bins::join_::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::join_::Base const& it) {
    visitCommon(it, std::conditional<!bins::join_::Base::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::join_::Base::Base const& it) {
    visitCommon(it, std::conditional<!bins::join_::Base::Base::args, std::true_type, std::false_type>::type{});
  }

  void VisRepr::visit(bins::map_ const& it) {
    visitCommon(it, std::conditional<!bins::map_::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::map_::Base const& it) {
    visitCommon(it, std::conditional<!bins::map_::Base::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::map_::Base::Base const& it) {
    visitCommon(it, std::conditional<!bins::map_::Base::Base::args, std::true_type, std::false_type>::type{});
  }

  void VisRepr::visit(bins::nl_ const& it) {
    visitCommon(it, std::conditional<!bins::nl_::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::nl_::Base const& it) {
    visitCommon(it, std::conditional<!bins::nl_::Base::args, std::true_type, std::false_type>::type{});
  }

  void VisRepr::visit(bins::pi_ const& it) {
    visitCommon(it, std::conditional<!bins::pi_::args, std::true_type, std::false_type>::type{});
  }

  void VisRepr::visit(bins::repeat_ const& it) {
    visitCommon(it, std::conditional<!bins::repeat_::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::repeat_::Base const& it) {
    visitCommon(it, std::conditional<!bins::repeat_::Base::args, std::true_type, std::false_type>::type{});
  }

  void VisRepr::visit(bins::replicate_ const& it) {
    visitCommon(it, std::conditional<!bins::replicate_::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::replicate_::Base const& it) {
    visitCommon(it, std::conditional<!bins::replicate_::Base::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::replicate_::Base::Base const& it) {
    visitCommon(it, std::conditional<!bins::replicate_::Base::Base::args, std::true_type, std::false_type>::type{});
  }

  void VisRepr::visit(bins::reverse_ const& it) {
    visitCommon(it, std::conditional<!bins::reverse_::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::reverse_::Base const& it) {
    visitCommon(it, std::conditional<!bins::reverse_::Base::args, std::true_type, std::false_type>::type{});
  }

  void VisRepr::visit(bins::singleton_ const& it) {
    visitCommon(it, std::conditional<!bins::singleton_::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::singleton_::Base const& it) {
    visitCommon(it, std::conditional<!bins::singleton_::Base::args, std::true_type, std::false_type>::type{});
  }

  void VisRepr::visit(bins::split_ const& it) {
    visitCommon(it, std::conditional<!bins::split_::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::split_::Base const& it) {
    visitCommon(it, std::conditional<!bins::split_::Base::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::split_::Base::Base const& it) {
    visitCommon(it, std::conditional<!bins::split_::Base::Base::args, std::true_type, std::false_type>::type{});
  }

  void VisRepr::visit(bins::sub_ const& it) {
    visitCommon(it, std::conditional<!bins::sub_::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::sub_::Base const& it) {
    visitCommon(it, std::conditional<!bins::sub_::Base::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::sub_::Base::Base const& it) {
    visitCommon(it, std::conditional<!bins::sub_::Base::Base::args, std::true_type, std::false_type>::type{});
  }

  void VisRepr::visit(bins::take_ const& it) {
    visitCommon(it, std::conditional<!bins::take_::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::take_::Base const& it) {
    visitCommon(it, std::conditional<!bins::take_::Base::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::take_::Base::Base const& it) {
    visitCommon(it, std::conditional<!bins::take_::Base::Base::args, std::true_type, std::false_type>::type{});
  }

  void VisRepr::visit(bins::takewhile_ const& it) {
    visitCommon(it, std::conditional<!bins::takewhile_::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::takewhile_::Base const& it) {
    visitCommon(it, std::conditional<!bins::takewhile_::Base::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::takewhile_::Base::Base const& it) {
    visitCommon(it, std::conditional<!bins::takewhile_::Base::Base::args, std::true_type, std::false_type>::type{});
  }

  void VisRepr::visit(bins::tonum_ const& it) {
    visitCommon(it, std::conditional<!bins::tonum_::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::tonum_::Base const& it) {
    visitCommon(it, std::conditional<!bins::tonum_::Base::args, std::true_type, std::false_type>::type{});
  }

  void VisRepr::visit(bins::tostr_ const& it) {
    visitCommon(it, std::conditional<!bins::tostr_::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::tostr_::Base const& it) {
    visitCommon(it, std::conditional<!bins::tostr_::Base::args, std::true_type, std::false_type>::type{});
  }

  void VisRepr::visit(bins::zipwith_ const& it) {
    visitCommon(it, std::conditional<!bins::zipwith_::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::zipwith_::Base const& it) {
    visitCommon(it, std::conditional<!bins::zipwith_::Base::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::zipwith_::Base::Base const& it) {
    visitCommon(it, std::conditional<!bins::zipwith_::Base::Base::args, std::true_type, std::false_type>::type{});
  }
  void VisRepr::visit(bins::zipwith_::Base::Base::Base const& it) {
    visitCommon(it, std::conditional<!bins::zipwith_::Base::Base::Base::args, std::true_type, std::false_type>::type{});
  }

} // namespace sel
