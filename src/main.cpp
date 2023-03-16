#include "actions.hpp"
#include "options.hpp"

int main() {
  App app;

  sel::ref<bins::pi_::Head> pi0(app);
  auto pii = pi0->copy();
  cout << "pii :: " << pii->type() << endl;

  sel::ref<bins::tonum_::Head> tonum1(app);
  cout << "tonum1 :: " << tonum1->type() << endl;

  sel::ref<NumResult> zero(app, 0);
  sel::ref<NumResult> one(app, 0);

  sel::ref<bins::add_::Head> add2(app);
  cout << "add2 :: " << add2->type() << endl;
  sel::ref<bins::add_::Head::Next> add1 = (*add2)(zero);
  cout << "add1 :: " << add1->type() << endl;
  sel::ref<bins::add_::Head::Next::Next> add0 = (*add1)(one);
  cout << "add0 :: " << add0->type() << endl;

  cout << "result = " << add0->value() << endl;

  sel::ref<bins::id_::Head> id1(app);
  cout << "id1 :: " << id1->type() << endl;
  sel::ref<bins::add_::Head::Next::Next> id0 = (*id1)(add0);
  cout << "id0 :: " << id0->type() << endl;

  sel::ref<bins::const_::Head> const2(app);
  cout << "const2 :: " << const2->type() << endl;
  sel::ref<bins::const_::Head::Next> const1 = (*const2)(id0);
  cout << "const1 :: " << const1->type() << endl;

  return 0;
}

int main1(int argc, char* argv[]) {
  ios_base::sync_with_stdio(false);
  //cin.tie(nullptr);

  Options const opts(argc, argv);

  if (opts.lookup) lookup(opts.lookup_names);

  App app(opts.strict, opts.any_type);

  if (opts.filename) buildfile(app, opts.filename);
  else build(app, opts.script);

  if (opts.any_type) {
    cout << app << "\n#       :: " << app.type() << endl;
    return EXIT_SUCCESS;
  }

  if (opts.debug) {
    app.repr(cout);
    return EXIT_SUCCESS;
  }

  if (opts.typecheck) {
    return EXIT_SUCCESS;
  }

  if (opts.compile) {
    char** flags = opts.compile_flags;
    char* outfile = *flags++;
    cerr << "out: " << quoted(outfile);
    if (*flags) {
      cerr << ", flags:\n";
      while (*flags) cerr << "   " << quoted(*flags++) << "\n";
    } else cerr << ", no flags\n";
    throw NIYError("emit binary");
  }

  run(app);

  return EXIT_SUCCESS;
}
