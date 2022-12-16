#include "sel/visitors.hpp"

namespace sel {

    void VisHelp::visit(bins::abs_::Head const& it) { res << std::remove_reference<decltype(it)>::type::the::Base::Next::doc; }
    void VisHelp::visit(bins::add_::Head const& it) { res << std::remove_reference<decltype(it)>::type::the::Base::Next::doc; }
    void VisHelp::visit(bins::const_::Head const& it) { res << std::remove_reference<decltype(it)>::type::the::Base::Next::doc; }
    void VisHelp::visit(bins::flip_::Head const& it) { res << std::remove_reference<decltype(it)>::type::the::Base::Next::doc; }
    void VisHelp::visit(bins::id_::Head const& it) { res << std::remove_reference<decltype(it)>::type::the::Base::Next::doc; }
    void VisHelp::visit(bins::join_::Head const& it) { res << std::remove_reference<decltype(it)>::type::the::Base::Next::doc; }
    void VisHelp::visit(bins::map_::Head const& it) { res << std::remove_reference<decltype(it)>::type::the::Base::Next::doc; }
    void VisHelp::visit(bins::nl_::Head const& it) { res << std::remove_reference<decltype(it)>::type::the::Base::Next::doc; }
    void VisHelp::visit(bins::pi_::Head const& it) { res << std::remove_reference<decltype(it)>::type::the::Base::Next::doc; }
    void VisHelp::visit(bins::repeat_::Head const& it) { res << std::remove_reference<decltype(it)>::type::the::Base::Next::doc; }
    void VisHelp::visit(bins::split_::Head const& it) { res << std::remove_reference<decltype(it)>::type::the::Base::Next::doc; }
    void VisHelp::visit(bins::sub_::Head const& it) { res << std::remove_reference<decltype(it)>::type::the::Base::Next::doc; }
    void VisHelp::visit(bins::tonum_::Head const& it) { res << std::remove_reference<decltype(it)>::type::the::Base::Next::doc; }
    void VisHelp::visit(bins::tostr_::Head const& it) { res << std::remove_reference<decltype(it)>::type::the::Base::Next::doc; }
    void VisHelp::visit(bins::zipwith_::Head const& it) { res << std::remove_reference<decltype(it)>::type::the::Base::Next::doc; }

}
