#ifndef SEL_ENGINE_HPP
#define SEL_ENGINE_HPP

/**
 * Main classes involved an runtime: types of values and
 * the application.
 */

#include <sstream>
#include <tuple>
#include <vector>

#include "ll.hpp" // TODO
#include "utils.hpp"
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
    Type const ty;

    // used by janky visitor pattern, essentially manual v-table; use `make_visit_table`
    virtual VisitTable visit_table() const = 0;

  public:
    Val(Type&& ty): ty(std::forward<Type>(ty)) { }
    virtual ~Val() { }

    Type const& type() const { return ty; }
    virtual std::unique_ptr<Val> copy() const = 0;

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
  std::unique_ptr<To> coerse(std::unique_ptr<Val> from, Type const& to);
  // forward (needed in LstMapCoerse)
  template <>
  std::unique_ptr<Val> coerse<Val>(std::unique_ptr<Val> from, Type const& to);

  /**
   * Abstract class for `Num`-type compatible values.
   * Numbers can be converted to common representation
   * (double for now).
   */
  struct Num : Val {
    Num()
      : Val(Type::makeNum())
    { }
    virtual double value() = 0;
  };

  /**
   * Abstract class for `Str`-type compatible values.
   * Strings are "rewindable streams".
   */
  struct Str : Val { //, public std::istream
    Str(bool is_inf)
      : Val(Type::makeStr(is_inf))
    { }
    friend std::ostream& operator<<(std::ostream& out, Str& val) { return val.stream(out); }
    /**
     * Stream (compute, etc..) some bytes. [Must be non-empty]
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
    virtual std::ostream& entire(std::ostream& out) = 0; // XXX: could have a default impl (if not the only one)
  };

  /**
   * Abstract class for `Lst`-type compatible values.
   * Lists are more like "rewindable iterators". (Methods
   * `next` and `rewind`.)
   */
  struct Lst : Val { //, public std::iterator<std::input_iterator_tag, Val>
    Lst(Type&& type)
      : Val(std::forward<Type>(type))
    { }
    /**
     * Get value and move to next, or falsy handle if past-the-end.
     */
    virtual std::unique_ptr<Val> operator++() = 0;
  };

  /**
   * Abstract class for `Fun`-type compatible values.
   * Functions can be applied an argument to produce
   * a new value.
   */
  struct Fun : Val {
    Fun(Type&& type)
      : Val(std::forward<Type>(type))
    { }
    virtual std::unique_ptr<Val> operator()(std::unique_ptr<Val> arg) = 0;
  };


  struct LstMapCoerse : Lst {
  private:
    std::unique_ptr<Lst> v;
    size_t now_has;
    size_t has_size;

  public:
    LstMapCoerse(std::unique_ptr<Lst> v, std::vector<Type> const& to_has)
      : Lst(Type::makeLst(std::vector<Type>(to_has), v->type().is_inf(), v->type().is_tpl()))
      , v(move(v))
      , now_has(0)
      , has_size(ty.has().size())
    { }

    std::unique_ptr<Val> operator++() override {
      auto curr = ++(*v);
      if (!curr) return curr;
      if (has_size <= ++now_has)
        now_has = 0;
      return coerse<Val>(move(curr), ty.has()[now_has]);
    }

    std::unique_ptr<Val> copy() const override {
      return std::make_unique<LstMapCoerse>(val_cast<Lst>(v->copy()), ty.has());
    }

    Lst const& source() const { return *v; }

  protected:
    VisitTable visit_table() const override {
      return make_visit_table<decltype(this)>::function();
    }
  };


  // @thx: http://gabisoft.free.fr/articles/fltrsbf1.html (2 pages, this page 1)
  class Str_streambuf : public std::streambuf {
    std::unique_ptr<Str> v;
    std::string buffered;

  public:
    Str_streambuf(std::unique_ptr<Str> v): v(move(v)) { }

    int_type overflow(int_type) override;
    int_type underflow() override;
  };

} // namespace sel

#endif // SEL_ENGINE_HPP
