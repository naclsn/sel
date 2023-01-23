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

  if (string("::") == *names) {
    ++names;

    stringstream ss;
    while (*names) ss << *names++ << ' ';

    Type ty;
    ss >> ty;

    bool found = false;

    vector<string> vs;
    list_names(vs);
    for (auto const& name : vs) {
      auto* it = lookup_name(app, name);

      if (it->type() == ty) {
        if (found) cout << "\n";
        else found = true;

        cout << name << " :: " << it->type() << "\n\t";
        help(*it); // TODO: (if stdout tty) auto line break at window width?
        cout << endl;
      }
    }

    if (!found) {
      cerr << "None known with type: " << ty << "\n";
      exit(EXIT_FAILURE);
    }
  } // if "::"

  else {
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

#endif // SELI_ACTIONS_HPP
