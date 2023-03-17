#ifndef SEL_ENGINE_HPP
#define SEL_ENGINE_HPP

/**
 * Main classes involved an runtime: types of values and
 * the application.
 */

#include <sstream>
#include <tuple>
#include <vector>

#include "application.hpp"
#include "ll.hpp"
#include "visitors.hpp"

namespace sel {

  template <typename Vi>
  using visit_table_entry = typename Vi::Ret(Vi&, Val const*);

  template <typename PackItself> struct _VisitTable;
  template <typename ...Pack>
  struct _VisitTable<ll::pack<Pack...>> {
    typedef std::tuple<visit_table_entry<Pack>*...> type;
  };

  typedef _VisitTable<visitors_list>::type VisitTable;

  template <typename Va, typename PackItself> struct _make_visit_table;
  template <typename Va, typename ...Pack>
  struct _make_visit_table<Va, ll::pack<Pack...>> {
    constexpr inline static VisitTable function() {
      return {(visit_table_entry<Pack>*)visit<Pack, Va>...};
    }
  };

  template <typename VaP>
  struct make_visit_table {
    constexpr inline static VisitTable function() {
      return _make_visit_table<typename std::remove_pointer<VaP>::type, visitors_list>::function();
    }
  };

  /**
   * Abstract base class for every types of values. See
   * also `Num`, `Str`, `Lst`, `Fun`. Each of
   * these abstract class in turn describe the common
   * ground for values of a type.
   */
  struct Val {
  protected:
    handle<Val> h; // handle over itself
    Type const ty;

    // used by janky visitor pattern, essentially manual v-table; use `make_visit_table`
    virtual VisitTable visit_table() const = 0;

  public:
    Val(handle<Val> at, Type&& ty);
    virtual ~Val() { }

    handle<Val> operator&() { return h; }
    Type const& type() const { return ty; }
    virtual handle<Val> copy() const = 0;

    // delete itself, construct a value of type U in its slot
    template <typename U, typename ...Args>
    void hold(Args&&... args) { new U(h, std::forward<Args>(args)...); }
    // delete itself, the slot will be free for any new one
    void drop() { h.drop(); }

    template <typename Vi>
    typename Vi::Ret accept(Vi& visitor) const {
      auto visit = std::get<ll::pack_index<Vi, visitors_list>::value>(visit_table());
      return visit(visitor, this);
    }
  };

  /**
   * Coerse a value to a type. Returned pointer may be
   * the same or a newly allocated value.
   * For now this is a template function, but this might
   * not be enough with dynamically known types; may change
   * it for `coerse(Val val, Type to)`.
   */
  template <typename To>
  handle<To> coerse(App& app, handle<Val> from, Type const& to);
  // forward (needed in LstMapCoerse)
  template <>
  handle<Val> coerse<Val>(App& app, handle<Val> from, Type const& to);

  /**
   * Abstract class for `Num`-type compatible values.
   * Numbers can be converted to common representation
   * (double for now).
   */
  struct Num : Val {
    Num(handle<Num> at)
      : Val(at, Type::makeNum())
    { }
    virtual double value() = 0;
  };

  /**
   * Abstract class for `Str`-type compatible values.
   * Strings are "rewindable streams".
   */
  struct Str : Val { //, public std::istream
    Str(handle<Str> at, bool is_inf)
      : Val(at, Type::makeStr(is_inf))
    { }
    friend std::ostream& operator<<(std::ostream& out, Str& val) { return val.stream(out); }
    /**
     * Stream (compute, etc..) some bytes.
     */
    virtual std::ostream& stream(std::ostream& out) = 0;
    /**
     * `true` if there is no more bytes to stream. Calling
     * `stream` (or `entire`) at this point is probably undefined.
     */
    virtual bool end() = 0;
    /**
     * Stream the whole string of bytes.
     */
    virtual std::ostream& entire(std::ostream& out) = 0;
  };

  /**
   * Abstract class for `Lst`-type compatible values.
   * Lists are more like "rewindable iterators". (Methods
   * `next` and `rewind`.)
   */
  struct Lst : Val { //, public std::iterator<std::input_iterator_tag, Val>
    Lst(handle<Lst> at, Type&& type)
      : Val(at, std::forward<Type>(type))
    { }
    /**
     * Get (compute, etc..) the current value.
     */
    virtual handle<Val> operator*() = 0;
    /**
     * Move to (compute, etc..) the next value.
     */
    virtual Lst& operator++() = 0;
    /**
     * `true` if there is no next value to get. Calling
     * `next` at this point is probably undefined.
     * so a list can go 'one-past-end', that is it is ok
     * to call ++ and * when end is false, as soon as end
     * is true, the previous value (if any) was the last
     * one; it is in a 'one-past-end' state (ie. likely
     * invalid for any operation)
     * this means an empty list will be end true right away
     */
    virtual bool end() = 0;
  };

  /**
   * Abstract class for `Fun`-type compatible values.
   * Functions can be applied an argument to produce
   * a new value.
   */
  struct Fun : Val {
    Fun(handle<Fun> at, Type&& type)
      : Val(at, std::forward<Type>(type))
    { }
    virtual handle<Val> operator()(handle<Val> arg) = 0;
  };


  struct LstMapCoerse : Lst {
  private:
    handle<Lst> v;
    size_t now_has;
    size_t has_size;

  public:
    LstMapCoerse(handle<Lst> at, handle<Lst> v, std::vector<Type> const& to_has)
      : Lst(at, Type::makeLst(std::vector<Type>(to_has), v->type().is_inf(), v->type().is_tpl()))
      , v(v)
      , now_has(0)
      , has_size(ty.has().size())
    { }

    handle<Val> operator*() override {
      return coerse<Val>(h.app(), *(*v), ty.has()[now_has]);
    }
    Lst& operator++() override {
      ++(*v);
      if (has_size <= ++now_has)
        now_has = 0;
      return *this;
    }
    bool end() override { return v->end(); }

    handle<Val> copy() const override {
      return handle<LstMapCoerse>(h.app(), v->copy(), ty.has());
    }

    Lst const& source() const { return *v; }

  protected:
    VisitTable visit_table() const override {
      return make_visit_table<decltype(this)>::function();
    }
  };


  // @thx: http://gabisoft.free.fr/articles/fltrsbf1.html (2 pages, this page 1)
  class Str_streambuf : public std::streambuf {
    handle<Str> v;
    std::string buffered;

  public:
    Str_streambuf(handle<Str> v): v(v) { }
    Str_streambuf(Str_streambuf const& o): std::streambuf(o), v(o.v) { }

    // Str_streambuf& operator=(Str_streambuf const&) = delete;
    // Str_streambuf& operator=(Str_streambuf&& sis);

    int_type overflow(int_type) override;
    int_type underflow() override;
  };

} // namespace sel

#endif // SEL_ENGINE_HPP
