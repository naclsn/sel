#include "sel/parser.hpp"
#include "sel/visitors.hpp"

namespace sel {

    VisName::Ret VisName::visit(NumDefine const& it) { return it.getdoc().c_str(); }
    VisName::Ret VisName::visit(StrDefine const& it) { return it.getdoc().c_str(); }
    VisName::Ret VisName::visit(LstDefine const& it) { return it.getdoc().c_str(); }
    VisName::Ret VisName::visit(FunDefine const& it) { return it.getdoc().c_str(); }

} // namespace sel
