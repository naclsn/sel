#ifndef SELI_ACTIONS_HPP
#define SELI_ACTIONS_HPP

#include "buildapp.hpp"

void lookup(char const* const names[]) {
  if (!names || !*names) {
    vector<string> vs;
    list_names(vs);
    for (auto const& it : vs) cout << it << '\n';
    exit(EXIT_SUCCESS);
  }

  VisHelp help(cout);

  App app;
  while (*names) {
    auto* it = lookup_name(app, *names);
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

void run(App& app) {
  try { app.run(cin, cout); }

  catch (RuntimeError const& err) {
    cerr
      << "Runtime error: "
      << err.what() << '\n'
    ;
    exit(EXIT_FAILURE);
  }

  catch (BaseError const& err) {
    cerr
      << "Error (while running application): "
      << err.what() << '\n'
    ;
    exit(EXIT_FAILURE);
  }
}

void compile(App& app, char const* const* flags) {
  char const* outfile = *flags++;

  // for now module_name == outfile
  if (*flags) {
    cerr << ", flags:\n";
    while (*flags) cerr << "   " << quoted(*flags++) << "\n";
  } else cerr << ", no flags\n";

  VisCodegen codegen(outfile, app);

  cerr << "```llvm-ir\n";
  codegen.dump();
  cerr << "```\n";

  throw NIYError("emit binary");
}

#endif // SELI_ACTIONS_HPP
