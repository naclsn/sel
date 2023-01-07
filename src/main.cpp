#include "options.hpp"
#include "actions.hpp"

int main(int argc, char* argv[]) {
  Options const opts(argc, argv);

  if (opts.lookup) lookup(opts.lookup_names);

  App app;
  // if (opts.strict) app.strict_type = true;

  if (opts.filename) buildfile(app, opts.filename);
  else build(app, opts.script);

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
