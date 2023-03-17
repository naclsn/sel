#include "actions.hpp"
#include "options.hpp"

int main(int argc, char* argv[]) {
  ios_base::sync_with_stdio(false);
  //cin.tie(nullptr);

  Options const opts(argc, argv);
  if (opts.exit) return opts.exit_code;

  if (opts.lookup) return lookup(opts.lookup_names);

  App app(opts.strict, opts.any_type);

  int code = opts.filename
    ? buildfile(app, opts.filename)
    : build(app, opts.script);
  if (EXIT_SUCCESS != code) return code;

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
    cerr << "out: " << utils::quoted(outfile);
    if (*flags) {
      cerr << ", flags:\n";
      while (*flags) cerr << "   " << utils::quoted(*flags++) << "\n";
    } else cerr << ", no flags\n";
    throw NIYError("emit binary");
  }

  return run(app);
}
