#include "options.hpp"
#include "actions.hpp"

int main(int argc, char* argv[]) {
  Options const opts(argc, argv);

  if (opts.lookup) lookup(opts.lookup_names);

  App app(opts.strict, opts.any_type);

  if (opts.filename) buildfile(app, opts.filename);
  else build(app, opts.script);

  if (opts.any_type) {
    cout << app << "\n\t:: " << app.type() << endl;
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
    compile(app, opts.compile_flags);
    return EXIT_SUCCESS;
  }

  run(app);

  return EXIT_SUCCESS;
}
