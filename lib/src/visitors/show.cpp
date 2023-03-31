#include "sel/builtins.hpp"
#include "sel/parser.hpp"
#include "sel/utils.hpp"
#include "sel/visitors.hpp"

namespace sel {

    VisShow::Ret VisShow::visit(NumLiteral const& it) {
      return res << it.underlying();
    }

    VisShow::Ret VisShow::visit(StrLiteral const& it) {
      return res << ":" << utils::quoted(it.underlying(), true, false) << ":";
    }

    VisShow::Ret VisShow::visit(LstLiteral const& it) {
      res << "{ ";
      char const* sep = "";
      for (auto const& v : it.underlying()) {
        res << sep;
        sep = ", ";
        v->accept(*this);
      }
      return res << " }";
    }

    VisShow::Ret VisShow::visit(FunChain const& it) {
      char const* sep = "";
      for (auto const& f : it.underlying()) {
        res << sep;
        sep = ", ";
        f->accept(*this);
      }
      return res;
    }

    VisShow::Ret VisShow::visit(NumDefine const& it) {
      return res << it.getname();
    }

    VisShow::Ret VisShow::visit(StrDefine const& it) {
      return res << it.getname();
    }

    VisShow::Ret VisShow::visit(LstDefine const& it) {
      return res << it.getname();
    }

    VisShow::Ret VisShow::visit(FunDefine const& it) {
      return res << it.getname();
    }

    VisShow::Ret VisShow::visit(Input const& it) {
      // YYY: really
      throw TypeError("show(LstMapCoerse) does not make sense");
    }

    VisShow::Ret VisShow::visit(NumResult const& it) {
      return res << it.result();
    }

    VisShow::Ret VisShow::visit(StrChunks const& it) {
      std::ostringstream oss;
      for (auto const& s : it.chunks())
        oss << utils::quoted(s, true, false);
      return res << ":" << oss.str() << ":";
    }

    VisShow::Ret VisShow::visit(LstMapCoerse const& it) {
      return it.source().accept(*this);
    }

} // namespace sel
