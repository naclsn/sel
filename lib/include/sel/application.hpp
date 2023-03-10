#ifndef SEL_APPLICATION_HPP
#define SEL_APPLICATION_HPP

#include <string>
#include <vector>
#include <unordered_map>

#include "forward.hpp"
#include "visitors.hpp"

namespace sel {

  /**
   * Manages allocated values.
   */
  class Valloc {
  private:
    // ownership of an allocated value
    struct slot {
    private:
      Val const* v = nullptr;

    public:
      slot(Val const* v): v(v) { }

      void change(Val const*);
      void drop();
      operator bool() const { return v; }
      explicit operator Val const*() const { return v; }
    };

    std::vector<slot> w;
    size_t free = 0;

  public:
    Valloc() { }

    // non-owning handle to a slot
    struct handle {
    private:
      friend Valloc;
      size_t i;
      Valloc& va;
      handle(Valloc& va, size_t i): i(i), va(va) { }
      operator size_t() { return i; }

    public:
      Val const* operator*() { return va[*this]; }
    };

    handle hold(Val const*);
    void change(handle, Val const*);
    void drop(handle);
    Val const* operator[](handle);

    void clear();
  };

  /**
   * An application is constructed from parsing a user
   * script. It serializes back to an equivalent script
   * (although it may not be strictly equal).
   */
  class App {
  private:
    Val* f; // note that this does not have to be `Str -> Str`
    std::unordered_map<std::string, Val*> user;

    Valloc va;

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

    Valloc::handle hold(Val const* v) { return va.hold(v); }
    void change(Valloc::handle at, Val const* v) { va.change(at, v); }
    void drop(Valloc::handle at) { va.drop(at); }
    void clear() { va.clear(); }

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
