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

void compile(App& app, char const* infile, char const* const* flags) {
  char const* outfile = *flags++;

  cerr << "outfile: " << quoted(outfile);

  if (*flags) {
    cerr << ", flags:\n";
    while (*flags) cerr << "   " << quoted(*flags++) << "\n";
  } else cerr << ", no flags\n";

  try {
    std::string tmp = outfile;
    auto st = tmp.rfind('/')+1;
    auto ed = tmp.rfind('.');
    std::string name = tmp.substr(st, ed-st);

    VisCodegen codegen(infile, name.c_str(), app);
    auto res = codegen.makeOutput();
    cerr << "\nmakeOutput: " << res << "\n\n";

    cerr << "```llvm-ir\n";
    codegen.dump(cerr);
    cerr << "```\n\n";

    codegen.dothething(outfile);
  }

  catch (CodegenError const& err) {
    cerr
      << "Codegen error: "
      << err.what() << '\n'
    ;
    exit(EXIT_FAILURE);
  }

  catch (BaseError const& err) {
    cerr
      << "Error (while compiling application): "
      << err.what() << '\n'
    ;
    exit(EXIT_FAILURE);
  }
}

#endif // SELI_ACTIONS_HPP
