#include "value.hpp"
#include "coerse.hpp"

namespace sel {

  std::ostream& Type::output(std::ostream& out) const {
    switch (base) {
      case Ty::UNK:
        out << *pars.name;
        break;

      case Ty::NUM:
        out << "Num";
        break;

      case Ty::STR:
        out << "Str";
        break;

      case Ty::LST:
        out << "[" << *pars.pair[0] << "]";
        break;

      case Ty::FUN:
        if (Ty::FUN == pars.pair[0]->base)
          out << "(" << *pars.pair[0] << ") -> " << *pars.pair[1];
        else
          out << *pars.pair[0] << " -> " << *pars.pair[1];
        break;

      case Ty::CPL:
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
    if (Ty::UNK == base || Ty::UNK == other.base)
      return true;

    if (base != other.base)
      return false;

    switch (base) {
      case Ty::NUM:
      case Ty::STR:
        return true;

      case Ty::LST:
        return pars.pair[0] == other.pars.pair[0];

      case Ty::FUN:
      case Ty::CPL:
        return pars.pair[0] == other.pars.pair[0]
            && pars.pair[1] == other.pars.pair[1];

      default:
        return false;
    }
  }

} // namespace sel
