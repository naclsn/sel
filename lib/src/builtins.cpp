#include "sel/builtins.hpp"
#include "sel/visitors.hpp"

namespace sel {

  template <typename list> struct linear_search;
  template <typename car, typename cdr>
  struct linear_search<bin_types::cons<car, cdr>> {
    static inline Val* the(std::string const& name) {
      if (car::name == name) return new typename car::Head();
      return linear_search<cdr>::the(name);
    }
  };
  template <>
  struct linear_search<bin_types::nil> {
    static inline Val* the(std::string const& _) {
      return nullptr;
    }
  };

  Val* lookup_name(std::string const& name) {
    return linear_search<bin_types::bins>::the(name);
  }

  template <typename NextT, typename to, typename from, typename from_again, typename from_more>
  void bins_helpers::_bin_be<NextT, ll::cons<to, ll::cons<from, ll::cons<from_again, from_more>>>>::accept(Visitor& v) const {
    v.visit(*this); // visitBody
  }

  template <typename NextT, typename last_to, typename last_from>
  void bins_helpers::_bin_be<NextT, ll::cons<last_to, ll::cons<last_from, ll::nil>>>::the::accept(Visitor& v) const{
    v.visit(*(typename Base::Next*)this); // visitTail
  }

  template <typename NextT, typename last_to, typename last_from>
  void bins_helpers::_bin_be<NextT, ll::cons<last_to, ll::cons<last_from, ll::nil>>>::accept(Visitor& v) const{
    v.visit(*this); // visitHead
  }

} // namespace sel
