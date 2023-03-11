#include "sel/application.hpp"
#include "sel/errors.hpp"
#include "sel/parser.hpp"
#include "sel/utils.hpp"
#include "sel/visitors.hpp"

#include <typeinfo>

namespace sel {

  Val* App::slot::peek() { return v; }
  Val* App::slot::peek() const { return v; }
  void App::slot::hold(Val* niw) { delete v; v = niw; }
  void App::slot::drop() { delete v; v = nullptr; }

  size_t App::reserve() {
    if (0 != free) {
      size_t k = 0;
      for (auto& it : w) {
        if (!it) {
          free--;
          TRACE("+ [" << k << "] (from free)");
          return k;
        }
        k++;
      }
      // turns out it did not have room left (this should not happen)
      free = 0;
    }
    w.emplace_back();
    TRACE("+ [" << w.size()-1 << "] (append new)");
    return w.size()-1;
  }

  void App::clear() {
    // size_t k = w.size();
    TRACE("- [..] (clearing)");
    for (auto it = w.rbegin(); it != w.rend(); ++it) {
      // TRACE("- [" << --k << "] (clearing)");
      it->drop();
    }
    w.clear();
  }

  Type const& App::type() const { return f->type(); }

  ref<Val> App::lookup_name_user(std::string const& name) {
    try {
      return user.at(name)->copy();
    } catch (.../*std::out_of_range const&*/) {
      return ref<Val>(*this, nullptr);
    }
  }
  void App::define_name_user(std::string const& name, ref<Val> v) {
    user.insert({name, v});
  }

  void App::run(std::istream& in, std::ostream& out) {
    Type const& ty = f->type();

    if (Ty::FUN != ty.base()) {
      std::ostringstream oss;
      throw TypeError((oss << "value of type " << ty << " is not a function", oss.str()));
    }

    TRACE("run, before stuff");
    ref<Val> valin = coerse<Val>(*this, ref<Input>(*this, in), ty.from());
    ref<Val> valout = (*(ref<Fun>)f)(valin);
    TRACE("run, after stuff");

    Str& res = *coerse<Str>(*this, valout, Type::makeStr(true));
    TRACE("running");
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

      ref<Val> const val = it.second;
      switch (val->type().base()) {
        case Ty::NUM: {
            NumDefine const& def = *(ref<NumDefine>)val;
            name = def.getname();
            doc = def.getdoc();
            u = &def.underlying();
          } break;
        case Ty::STR: {
            StrDefine const& def = *(ref<StrDefine>)val;
            name = def.getname();
            doc = def.getdoc();
            u = &def.underlying();
          } break;
        case Ty::LST: {
            LstDefine const& def = *(ref<LstDefine>)val;
            name = def.getname();
            doc = def.getdoc();
            u = &def.underlying();
          } break;
        case Ty::FUN: {
            FunDefine const& def = *(ref<FunDefine>)val;
            name = def.getname();
            doc = def.getdoc();
            u = &def.underlying();
          } break;
        default: {
          std::ostringstream oss;
          throw TypeError((oss << "unexpected type in def expression: '" << val->type() << "'", oss.str()));
        }
      }

      // TODO: use word wrapping at eg. 80 char?
      out
        << "def " << name << ":\n  "
        << quoted(doc, true, false) << ":\n  ["
      ;
      u->accept(show) << "]\n  ;\n";
    }
    return app.f->accept(show);
  }

  std::istream& operator>>(std::istream& in, App& app) {
    ref<Val> tmp = parseApplication(app, in);
    app.f = app.not_fun
      ? tmp
      : (ref<Val>)coerse<Fun>(app, tmp, tmp->type());
    return in;
  }

} // namespace sel
