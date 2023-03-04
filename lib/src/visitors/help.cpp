#include "sel/parser.hpp"
#include "sel/visitors.hpp"

namespace sel {

    VisHelp::Ret VisHelp::visit(NumDefine const& it) { return it.getdoc().c_str(); }
    VisHelp::Ret VisHelp::visit(StrDefine const& it) { return it.getdoc().c_str(); }
    VisHelp::Ret VisHelp::visit(LstDefine const& it) { return it.getdoc().c_str(); }
    VisHelp::Ret VisHelp::visit(FunDefine const& it) { return it.getdoc().c_str(); }

} // namespace sel
