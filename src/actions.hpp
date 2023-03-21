#ifndef SELI_ACTIONS_HPP
#define SELI_ACTIONS_HPP

#include "sel/builtins.hpp"
#include "sel/utils.hpp"

#include "buildapp.hpp"
#include "termutils.hpp"
#include "wordwrap.hpp"

int lookup(char const* const names[]) {
  if (!names || !*names) {
    for (size_t k = 0; k < bins_list::count; k++)
      cout << bins_list::names[k] << '\n';
    return EXIT_SUCCESS;
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
      auto it = lookup_name(name);

      if (it->type() == ty && it->type().arity() == ar) {
        if (found) cout << "\n";
        else found = true;

        cout << name << " :: " << it->type() << "\n";
        wwcout << it->accept(help) << endl;
      }
    }

    if (!found) {
      cerr << "None known with type: " << ty << "\n";
      return EXIT_FAILURE;
    }
  } // if "::"

  else {
    vector<char const*> not_found;

    while (*names) {
      auto it = lookup_name(*names);
      if (it) {
        cout << *names << " :: " << it->type() << "\n";
        wwcout << it->accept(help) << endl;
      } else not_found.push_back(*names);

      if (*++names) cout << endl;
    }

    if (!not_found.empty()) {
      cerr << "Unknown name" << (1 == not_found.size() ? "" : "s") << ":";
      for (auto const& it : not_found)
        cerr << " " << utils::quoted(it);
      cerr << "\n";
      return EXIT_FAILURE;
    }
  } // if not "::"

  return EXIT_SUCCESS;
}

int run(App& app) {
  try { app.run(cin, cout); }

  catch (RuntimeError const& err) {
    cerr
      << "Runtime error: "
      << err.what() << '\n'
    ;
    return EXIT_FAILURE;
  }

  catch (BaseError const& err) {
    cerr
      << "Error (while running application): "
      << err.what() << '\n'
    ;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

#endif // SELI_ACTIONS_HPP
