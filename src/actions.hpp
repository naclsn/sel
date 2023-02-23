#ifndef SELI_ACTIONS_HPP
#define SELI_ACTIONS_HPP

#include "buildapp.hpp"
#include "options.hpp"

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

void compile(App& app, char const* infile, Options const& opts) {

  cerr << "infile: " << infile << "\n";
  cerr << "outfile: " << opts.outfile << "\n";
  cerr << "funname: " << (opts.funname ? opts.funname : "-nil-") << "\n";
  cerr << "no_link: " << opts.no_link << "\n";
  cerr << "no_assemble: " << opts.no_assemble << "\n";

  std::string tmp = opts.outfile;
  auto st = tmp.rfind('/')+1;
  auto ed = tmp.rfind('.');
  std::string name = tmp.substr(st, ed-st);

  try {
    // if no name provided, used the module name as function name
    char const* used_funname = opts.funname ? opts.funname : name.c_str();
    VisCodegen codegen(infile, name.c_str(), used_funname, app);

    if (!opts.funname) codegen.makeMain();

    if (opts.no_assemble) {
      std::ofstream ll(opts.outfile);
      if (!ll.is_open()) throw BaseError("could not open file");
      codegen.print(ll);

    } else {
      codegen.compile(opts.outfile, !opts.no_link);
    }
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
