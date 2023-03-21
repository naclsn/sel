#include "sel/application.hpp"
#include "sel/errors.hpp"
#include "sel/parser.hpp"
#include "sel/utils.hpp"
#include "sel/visitors.hpp"

using namespace std;

namespace sel {

  Type const& App::type() const { return f->type(); }

  unique_ptr<Val> App::lookup_name_user(string const& name) {
    try {
      return user.at(name)->copy();
    } catch (.../*out_of_range const&*/) {
      return nullptr;
    }
  }
  void App::define_name_user(string const& name, unique_ptr<Val> v) {
    user.emplace(name, move(v));
  }

  void App::run(istream& in, ostream& out) {
    Type const& ty = f->type();

    auto asfun = coerse<Fun>(move(f), Type::makeFun(Type::makeStr(true), Type::makeStr(true)));
    TRACE("run, before stuff");
    // auto valin = coerse<Val>(unique_ptr<Val>((Val*)new Input(in)), ty.from());
    auto valin = coerse<Val>(make_unique<Input>(in), ty.from());
    auto valout = (*asfun)(move(valin));
    TRACE("run, after stuff");

    auto res = coerse<Str>(move(valout), Type::makeStr(true));
    TRACE("running");
    res->entire(out);
  }

  void App::repr(ostream& out, VisRepr::ReprCx cx) const {
    VisRepr::ReprCx ccx = {
      .indents= cx.indents+1,
      .top_level= false,
      .single_line= cx.single_line
    };
    VisRepr repr(out, ccx);

    string ind;
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

  ostream& operator<<(ostream& out, App const& app) {
    VisShow show(out);

    // XXX: probably the map needs to be ordered because a 'def' can depend on an other
    for (auto const& it : app.user) {
      string name;
      string doc;
      Val const* u;

      unique_ptr<Val> const& val = it.second;
      switch (val->type().base()) {
        case Ty::NUM: {
            NumDefine const& def = *(NumDefine*)val.get();
            name = def.getname();
            doc = def.getdoc();
            u = &def.underlying();
          } break;
        case Ty::STR: {
            StrDefine const& def = *(StrDefine*)val.get();
            name = def.getname();
            doc = def.getdoc();
            u = &def.underlying();
          } break;
        case Ty::LST: {
            LstDefine const& def = *(LstDefine*)val.get();
            name = def.getname();
            doc = def.getdoc();
            u = &def.underlying();
          } break;
        case Ty::FUN: {
            FunDefine const& def = *(FunDefine*)val.get();
            name = def.getname();
            doc = def.getdoc();
            u = &def.underlying();
          } break;
        default: {
          ostringstream oss;
          throw TypeError((oss << "unexpected type in def expression: '" << val->type() << "'", oss.str()));
        }
      }

      // TODO: use word wrapping at eg. 80 char?
      out
        << "def " << name << ":\n  "
        << utils::quoted(doc, true, false) << ":\n  ["
      ;
      u->accept(show) << "]\n  ;\n";
    }
    return app.f->accept(show);
  }

  istream& operator>>(istream& in, App& app) {
    unique_ptr<Val> tmp = parseApplication(app, in);
    if (app.not_fun) {
      Type const& ty = tmp->type();
      app.f = coerse<Fun>(move(tmp), ty);
    } else app.f = move(tmp);
    return in;
  }

} // namespace sel
