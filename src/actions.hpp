#ifndef SELI_ACTIONS_HPP
#define SELI_ACTIONS_HPP

#include "buildapp.hpp"
#include "options.hpp"
#include "termutils.hpp"
#include "wordwrap.hpp"

void lookup(char const* const names[]) {
  if (!names || !*names) {
    for (size_t k = 0; k < bins_list::count; k++)
      cout << bins_list::names[k] << '\n';
    exit(EXIT_SUCCESS);
  }

  unsigned w, h;
  int a = term_size(w, h);

  wordwrap<3> wwcout(0 == a ? w : -1, cout);
  VisHelp help;

  App app;

  if (string("::") == *names) {
    ++names;

    stringstream ss;
    while (*names) ss << *names++ << ' ';

    Type ty;
    ss >> ty;
    unsigned ar = ty.arity();

    bool found = false;

    for (size_t k = 0; k < bins_list::count; k++) {
      char const* name = bins_list::names[k];
      auto* it = lookup_name(app, name);

      if (it->type() == ty && it->type().arity() == ar) {
        if (found) cout << "\n";
        else found = true;

        cout << name << " :: " << it->type() << "\n";
        wwcout << it->accept(help) << endl;
      }
    }

    if (!found) {
      cerr << "None known with type: " << ty << "\n";
      exit(EXIT_FAILURE);
    }
  } // if "::"

  else {
    vector<char const*> not_found;

    while (*names) {
      auto* it = lookup_name(app, *names);
      if (it) {
        cout << *names << " :: " << it->type() << "\n";
        wwcout << it->accept(help) << endl;
      } else not_found.push_back(*names);

      if (*++names) cout << endl;
      delete it;
    }

    if (!not_found.empty()) {
      cerr << "Unknown name" << (1 == not_found.size() ? "" : "s") << ":";
      for (auto const& it : not_found)
        cerr << " " << quoted(it);
      cerr << "\n";
      exit(EXIT_FAILURE);
    }
  } // if not "::"

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

  // if no name provided, used the module name as function name
  char const* used_funname = opts.funname ? opts.funname : name.c_str();
  VisCodegen codegen(infile, name.c_str(), used_funname, app);

  try {
    codegen.makeModule();

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

    // still tries to dump (if -S)
    if (opts.no_assemble) {
      std::ofstream ll(opts.outfile);
      if (!ll.is_open()) throw BaseError("could not open file");
      codegen.print(ll);
    }

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
