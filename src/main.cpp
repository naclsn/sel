#include <iostream>
#include <sstream>

#include "sel/errors.hpp"
#include "sel/parser.hpp"

using namespace std;
using namespace sel;

void usage(char const* prog, const char* reason) {
  if (reason) cerr << "Error: " << reason << "\n";
  cerr // TODO: better/proper (+ eg. man page)
    << "Usage: " << prog << " [-Dc] <script...>\n"
    << "       " << prog << " -h\n"
    << "       " << prog << " -l [<names...>]\n"
  ;
  exit(EXIT_FAILURE);
}

void lookup(char const* const names[]) {
  if (!*names) {
    vector<string> vs;
    list_names(vs);
    for (auto const& it : vs) cout << it << '\n';
    exit(EXIT_SUCCESS);
  }

  VisHelp help(cout);

  while (*names) {
    auto* it = lookup_name(*names);
    if (!it) {
      cerr << "Unknown name " << *names << "\n";
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

void build(App& app, char const* const srcs[]) {
  stringstream source;
  while (*srcs) source << *srcs++ << ' ';

  try { source >> app; }

  catch (ParseError const& err) {
    cerr
      << "Error: (building application) "
      << err.what() << '\n'
      << "at: " << source.str() << '\n'
      << "    " << string(err.start, ' ') << string(err.span, '~') << '\n'
    ;
    exit(EXIT_FAILURE);
  }

  catch (BaseError const& err) {
    cerr
      << "Error: (building application) "
      << err.what() << '\n'
    ;
    exit(EXIT_FAILURE);
  }
}

int main1(int argc, char const* const argv[]) {
  char const* prog = *argv++;
  if (argc < 2) usage(prog, "missing script");

  // ...
  bool cla_debug = false;
  bool cla_typecheck = false;
  while (*argv && '-' == (*argv)[0]) {
    char c = (*argv)[1];
    switch (c) {
      case 'h': usage(prog, NULL); break;
      case 'l': lookup(++argv); break;
      case 'D': cla_debug = true;     argv++; break;
      case 'c': cla_typecheck = true; argv++; break;
      default:
        if (c < '0' || '9' < c) usage(prog, (string("unknown flag '")+c+"'").c_str());
        goto after_while;
    }
  }
  if (!*argv) usage(prog, "missing script");
after_while: ;

  App app;
  build(app, argv);

  if (cla_debug) {
    app.repr(cout);
    return EXIT_SUCCESS;
  }

  if (cla_typecheck) {
    throw NIYError("type checking of user script");
    return argc & 1;
  }

  app.run(cin, cout);

  return EXIT_SUCCESS;
}

int main(void) {
  Fun* abs = new bins::abs_::Head();
  Num* abs42 = (Num*)(*abs)(new NumLiteral(-42));

  Fun* add = new bins::add_::Head();
  Fun* add_abs42 = (Fun*)(*add)(abs42);

  Num* pi = new bins::pi_::Head();
  Num* add_abs42_pi = (Num*)(*add_abs42)(pi);

  Val* aap = add_abs42_pi->copy();
  cout << "\n=== aap: " << repr(*aap) << "\n";

  Fun* const_ = new bins::const_::Head();
  Val* constt = const_->copy();
  cout << "\n=== constt: " << repr(*constt) << "\n";

  Val* const_add_abs42_pi = (*const_)(add_abs42_pi);
  Val* caap = const_add_abs42_pi->copy();
  cout << "\n=== caap: " << repr(*caap) << "\n";

  Fun* id = new bins::id_::Head();
  Val* idd = id->copy();
  cout << "\n=== idd: " << repr(*idd) << "\n";

  cout << "coucou\n";
}
