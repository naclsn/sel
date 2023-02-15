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

  cerr << "outfile: " << quoted(outfile);

  if (*flags) {
    cerr << ", flags:\n";
    while (*flags) cerr << "   " << quoted(*flags++) << "\n";
  } else cerr << ", no flags\n";

  // for now module_name == outfile
  VisCodegen codegen(outfile, app);
  auto res = codegen.makeOutput();
  cerr << "\nmakeOutput: " << res << "\n\n";

  cerr << "```llvm-ir\n";
  codegen.dump(cerr);
  cerr << "```\n\n";

  // codegen.dothething(outfile);
  { // temporary dothething
    std::string n = outfile;

    {
      ofstream ll(n + ".ll");
      if (!ll.is_open()) throw BaseError("could not open file");
      codegen.dump(ll);
      ll.flush();
      ll.close();
    }

    // $ llc -filetype=obj hello-world.ll -o hello-world.o
    {
      ostringstream oss;
      int r = std::system((oss << "llc -filetype=obj " << n << ".ll -o " << n << ".o", oss.str().c_str()));
      cerr << "============ " << r << "\n";
      if (0 != r) throw BaseError("see above");
    }

    // $ clang hello-world.o -o hello-world
    {
      ostringstream oss;
      int r = std::system((oss << "clang " << n << ".o -o " << n, oss.str().c_str()));
      cerr << "============ " << r << "\n";
      if (0 != r) throw BaseError("see above");
    }
  }
}

#endif // SELI_ACTIONS_HPP
