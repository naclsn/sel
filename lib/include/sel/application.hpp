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
  public:
    // non-owning handle to a slot
    template <typename U>
    struct handle {
    private:
      friend App;
      size_t i;
      App& va;
      handle(App& va, size_t i): i(i), va(va) { }
      // explicit operator size_t() { return i; }

    public:
      template <typename V>
      handle(handle<V> const& o): i(o.i), va(o.va) { }

      handle& operator=(handle const& o) { i = o.i; /*va = o.va;*/ return *this; }

      template <typename V>
      handle& operator=(handle<V> const& o) { i = o.i; /*va = o.va;*/ return *this; }

      template <typename ...Args>
      handle(App& app, Args&&... args)
        : i(app.reserve())
        , va(app)
      { new U(*this, std::forward<Args>(args)...); }

      // XXX: this might be a temporary solution
      // except if i don't search for an other one
      handle(App& app, std::nullptr_t): i(-1), va(app) { }
      operator bool() const { return (size_t)-1 != i; }
      bool operator!() const { return (size_t)-1 == i; }

      // used once to fill in the allocated slot at value construction
      void hold(U* v) { va.w[i].hold(v); }

      // helper to get the App back
      App& app() const { return va; }

      // accessors to underlying value
      U& operator*() { return *(U*)va.w[i].peek(); }
      U* operator->() { return (U*)va.w[i].peek(); }
      U const& operator*() const { return *(U*)va.w[i].peek(); }
      U const* operator->() const { return (U*)va.w[i].peek(); }
    };

  private:
    App::handle<Val> f; // note that this does not have to be `Str -> Str`
    std::unordered_map<std::string, App::handle<Val>> user;

    // ownership of an allocated value
    struct slot {
    private:
      Val* v = nullptr;

    public:
      slot(): v(nullptr) { }
      slot(Val* v): v(v) { }

      Val* peek();
      void hold(Val*);
      void drop();
      operator bool() const { return v; }
      // explicit operator Val*() const { return v; }
    };

    std::vector<slot> w;
    size_t free = 0;

    size_t reserve();

    bool strict_type = false;
    bool not_fun = false; // ie yes fun by default // YYY: probably remove

  public:
    App(): f(App::handle<Val>(*this, nullptr)) { }
    App(bool strict_type, bool not_fun)
      : f(App::handle<Val>(*this, nullptr))
      , strict_type(strict_type)
      , not_fun(not_fun)
    { }
    App(App const&) = delete;
    App(App&&) = delete;
    ~App() { clear(); }

    // template <typename U>
    // handle<U> reserve() { return handle<U>(*this, p_reserve()); }
    // template <typename U>
    // void change(handle<U> at, Val* niw) { p_change((size_t)at, niw); }
    // template <typename U>
    // void drop(handle<U> at) { p_drop((size_t)at); }
    // template <typename U>
    // Val* operator[](handle<U> at) { return p_at((size_t)at); }
    // same with const..?

    void clear();

    Type const& type() const;

    bool is_strict_type() const { return strict_type; }

    App::handle<Val> lookup_name_user(std::string const& name);
    void define_name_user(std::string const& name, App::handle<Val> v);

    void run(std::istream& in, std::ostream& out);

    // note: it always assumes `top_level`, even if false (ie. no single line)
    void repr(std::ostream& out, VisRepr::ReprCx cx={.top_level=true}) const;

    friend std::ostream& operator<<(std::ostream& out, App const& ty);
    friend std::istream& operator>>(std::istream& in, App& tt);
  };

  template <typename U>
  using ref = App::handle<U>;

} // namespace sel

#endif // SEL_APPLICATION_HPP
