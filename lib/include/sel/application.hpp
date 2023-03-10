#ifndef SEL_APPLICATION_HPP
#define SEL_APPLICATION_HPP

#include <string>
#include <vector>
#include <unordered_map>

#include "forward.hpp"
#include "visitors.hpp"

namespace sel {

  /**
   * An application is constructed from parsing a user
   * script. It serializes back to an equivalent script
   * (although it may not be strictly equal).
   */
  class App {
  private:
    Val* f; // note that this does not have to be `Str -> Str`
    std::unordered_map<std::string, Val*> user;

    std::vector<Val const*> ptrs;

    bool strict_type = false;
    bool not_fun = false; // ie yes fun by default // YYY: probably remove

  public:
    App() { }
    App(bool strict_type, bool not_fun)
      : strict_type(strict_type)
      , not_fun(not_fun)
    { }
    App(App const&) = delete;
    App(App&&) = delete;
    ~App() { clear(); }

    void push(Val const* v);
    void remove(Val const* v);
    void clear();

    Type const& type() const;

    bool is_strict_type() const { return strict_type; }

    Val* lookup_name_user(std::string const& name);
    void define_name_user(std::string const& name, Val* v);

    void run(std::istream& in, std::ostream& out);

    // note: it always assumes `top_level`, even if false (ie. no single line)
    void repr(std::ostream& out, VisRepr::ReprCx cx={.top_level=true}) const;

    friend std::ostream& operator<<(std::ostream& out, App const& ty);
    friend std::istream& operator>>(std::istream& in, App& tt);
  };

} // namespace sel

#endif // SEL_APPLICATION_HPP
