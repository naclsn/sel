#include "sel/application.hpp"
#include "sel/errors.hpp"
#include "sel/parser.hpp"
#include "sel/utils.hpp"
#include "sel/visitors.hpp"

namespace sel {

  void Valloc::slot::change(Val const* niw) { delete v; v = niw; }
  void Valloc::slot::drop() { delete v; v = nullptr; }

  Valloc::handle Valloc::hold(Val const* niw) {
    if (0 != free) {
      size_t k = 0;
      for (auto& it : w) {
        if (!it) {
          it.change(niw);
          free--;
          return handle(*this, k);
        }
        k++;
      }

      // turns out it did not have room left (this should not happen)
      free = 0;
    }

    w.emplace_back(niw);
    return handle(*this, w.size()-1);
  }

  void Valloc::change(handle at, Val const* niw) {
    w[at].change(niw);
  }

  void Valloc::drop(handle at) {
    w[at].drop();
    free++;
  }

  Val const* Valloc::operator[](handle at) {
    return (Val const*)w[at];
  }

  void Valloc::clear() {
    for (auto it = w.rbegin(); it != w.rend(); ++it)
      it->drop();
    w.clear();
  }

  Type const& App::type() const { return f->type(); }

  Val* App::lookup_name_user(std::string const& name) {
    try {
      return user.at(name)->copy();
    } catch (.../*std::out_of_range const&*/) {
      return nullptr;
    }
  }
  void App::define_name_user(std::string const& name, Val* v) {
    user[name] = v;
  }

  void App::run(std::istream& in, std::ostream& out) {
    Type const& ty = f->type();

    if (Ty::FUN != ty.base()) {
      std::ostringstream oss;
      throw TypeError((oss << "value of type " << ty << " is not a function", oss.str()));
    }

    Val* valin = coerse<Val>(*this, new Input(*this, in), ty.from());
    Val* valout = (*(Fun*)f)(valin);

    Str& res = *coerse<Str>(*this, valout, Type::makeStr(true));
    res.entire(out);
  }

  void App::repr(std::ostream& out, VisRepr::ReprCx cx) const {
    VisRepr::ReprCx ccx = {
      .indents= cx.indents+1,
      .top_level= false,
      .single_line= cx.single_line
    };
    VisRepr repr(out, ccx);

    std::string ind;
    ind.reserve(3 * ccx.indents);
    for (unsigned k = 0; k < ccx.indents; k++)
      ind.append("   ");

    out << "App {\n" << ind << "f= ";
    f->accept(repr);
    out << "\n";

    out << ind << "user= {";
    if (!user.empty()) {
      out << "\n" << ind;
      VisRepr repr(out, ccx);
      for (auto const& it : user) {
        out << ind << "[" << it.first << "]=";
        if (it.second) {
          out << it.second->type() << " ";
          it.second->accept(repr);
        } else out << " -nil-";
        out << "\n" << ind;
      }
    }
    out << "}\n";

    out << ind.substr(0, ind.length()-3) << "}\n";
  }

  std::ostream& operator<<(std::ostream& out, App const& app) {
    VisShow show(out);

    // XXX: probably the map needs to be ordered because a 'def' can depend on an other
    for (auto const& it : app.user) {
      std::string name;
      std::string doc;
      Val const* u;

      Val const* val = it.second;
      switch (val->type().base()) {
        case Ty::NUM: {
            NumDefine const& def = *(NumDefine*)val;
            name = def.getname();
            doc = def.getdoc();
            u = &def.underlying();
          } break;
        case Ty::STR: {
            StrDefine const& def = *(StrDefine*)val;
            name = def.getname();
            doc = def.getdoc();
            u = &def.underlying();
          } break;
        case Ty::LST: {
            LstDefine const& def = *(LstDefine*)val;
            name = def.getname();
            doc = def.getdoc();
            u = &def.underlying();
          } break;
        case Ty::FUN: {
            FunDefine const& def = *(FunDefine*)val;
            name = def.getname();
            doc = def.getdoc();
            u = &def.underlying();
          } break;
        default: {
          std::ostringstream oss;
          throw TypeError((oss << "unexpected type in def expression: '" << val->type() << "'", oss.str()));
        }
      }

      out
        << "def " << name << ":\n  "
        << quoted(doc, true, false) << ":\n  ["
      ;
      u->accept(show) << "]\n  ;\n";
    }
    return app.f->accept(show);
  }

  std::istream& operator>>(std::istream& in, App& app) {
    Val* tmp = parseApplication(app, in);
    app.f = app.not_fun
      ? tmp
      : coerse<Fun>(app, tmp, tmp->type());
    return in;
  }

} // namespace sel
