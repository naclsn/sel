#ifndef SEL_APPLICATION_HPP
#define SEL_APPLICATION_HPP

#include <string>
#include <vector>
#include <unordered_map>

#include "forward.hpp"
#include "utils.hpp" // ZZZ: TRACE
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
    public:
      size_t i;
      App& va;
      handle(App& va, size_t i): i(i), va(va) { }

    public:
      template <typename V>
      handle(handle<V> const& o): i(o.i), va(o.va) { }

      handle& operator=(handle const& o) { i = o.i; /*va = o.va;*/ return *this; }

      template <typename ...Args>
      handle(App& app, Args&&... args)
        : i(app.reserve())
        , va(app)
      { new U(*this, std::forward<Args>(args)...); }

      // XXX: this might be a temporary solution
      // except if i don't search for an other one
      handle(App& app, std::nullptr_t): i(-1), va(app) { }
      // note: a handle testing true does not mean it
      // references a slot which contains a value, but
      // that it does reference a slot that exists
      operator bool() const { return (size_t)-1 != i; }
      bool operator!() const { return (size_t)-1 == i; }
      // the whole testing as bool/constructing from
      // nullptr should be removed as soon as made
      // possible; it is used in place where either:
      // - option, ie. construction op may fail (parsing)
      // - delayed init, eg. in some bins, in parsing..

      // replace any previously held value with this one
      void hold(U* v) {
        if (nullptr == va.w[i].peek()) {
          TRACE("| [" << i << "] (was free)");
        } else {
          TRACE("| [" << i << "] (was " << utils::repr(**this, true, true, true) << ")");
        }
        va.w[i].hold(v); }
      // drop any previously held value
      void drop() {
        TRACE("- [" << i << "] (was " << utils::repr(**this, true, true, true) << ")");
        va.w[i].drop(); va.free++; }

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
      Val* peek() const;
      void hold(Val*);
      void drop();
      operator bool() const { return v; }
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
  using handle = App::handle<U>;

  static App _;
  // TODO: make it a real thing (eg. copy-able?, drop-able?)
  static handle<Val> null_handle(_, nullptr);

} // namespace sel

#endif // SEL_APPLICATION_HPP
