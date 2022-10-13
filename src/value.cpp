#include "value.hpp"
#include "coerse.hpp"

namespace sel {

//#pragma region Type
  std::ostream& Type::output(std::ostream& out) const {
    switch (base) {
      case BasicType::UNK:
        out << *pars.name;
        break;

      case BasicType::NUM:
        out << "Num";
        break;

      case BasicType::STR:
        out << "Str";
        break;

      case BasicType::LST:
        out << "[" << *pars.pair[0] << "]";
        break;

      case BasicType::FUN:
        if (BasicType::FUN == pars.pair[0]->base)
          out << "(" << *pars.pair[0] << ") -> " << *pars.pair[1];
        else
          out << *pars.pair[0] << " -> " << *pars.pair[1];
        break;

      case BasicType::CPL:
        out << "(" << *pars.pair[0] << ", " << *pars.pair[1] << ")";
        break;
    }
    return out;
  }
  std::ostream& operator<<(std::ostream& out, Type const& ty) { return ty.output(out); }

  /**
   * Two types compare equal if any:
   * - any UNK
   * - both STR or both NUM
   * - both LST and recursive on has
   * - both FUN and recursive on fst and snd
   * - both CPL and recursive on fst and snd
   */
  bool Type::operator==(Type const& other) const {
    if (BasicType::UNK == base || BasicType::UNK == other.base)
      return true;

    if (base != other.base)
      return false;

    switch (base) {
      case BasicType::NUM:
      case BasicType::STR:
        return true;

      case BasicType::LST:
        return pars.pair[0] == other.pars.pair[0];

      case BasicType::FUN:
      case BasicType::CPL:
        return pars.pair[0] == other.pars.pair[0]
            && pars.pair[1] == other.pars.pair[1];

      default:
        return false;
    }
  }
//#pragma endregion

//#pragma region Num
  Val const* Num::coerse(Type to) const {
    if (BasicType::NUM == to.base) return this;
    if (BasicType::STR == to.base) {
      Val* r = new CoerseNumToStr((Val*)this);
      return r;
    }
    throw TypeError(typed(), to, "Num::coerse");
  }
//#pragma endregion

//#pragma region Num
  Val const* Str::coerse(Type to) const {
    if (BasicType::STR == to.base) return this;
    TODO("Str::coerse");
    throw TypeError(typed(), to, "Str::coerse");
  }
//#pragma endregion

} // namespace sel
