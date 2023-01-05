#include <iostream>
#include <sstream>
#include <fstream>

#include "options.hpp"
#include "buildapp.hpp"

using namespace std;
using namespace sel;

void lookup(char const* const names[]) {
  if (!names || !*names) {
    vector<string> vs;
    list_names(vs);
    for (auto const& it : vs) cout << it << '\n';
    exit(EXIT_SUCCESS);
  }

  VisHelp help(cout);

  while (*names) {
    auto* it = lookup_name(*names);
    if (!it) {
      cerr << "Unknown name: " << quoted(*names) << "\n";
      exit(EXIT_FAILURE);
    }

    cout << *names << " :: " << it->type() << "\n\t";
    help(*it); // TODO: (if stdout tty) auto line break at window width?
    cout << endl;

    if (*++names) cout << endl;
    delete it;
  }

  exit(EXIT_SUCCESS);
}

int main(int argc, char* argv[]) {
  Options const opts(argc, argv);
  App app;

  if (opts.lookup) lookup(opts.lookup_names);

  if (opts.filename) buildfile(app, opts.filename);
  else build(app, opts.script);

  if (opts.debug) {
    app.repr(cout);
    return EXIT_SUCCESS;
  }

  if (opts.typecheck) {
    throw NIYError("type checking of user script");
    return EXIT_FAILURE;
  }

  app.run(cin, cout);

  return EXIT_SUCCESS;
}
