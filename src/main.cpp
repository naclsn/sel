#include <iostream>
#include <sstream>
#include <fstream>

#include "sel/errors.hpp"
#include "sel/parser.hpp"

using namespace std;
using namespace sel;

void usage(char const* prog, const char* reason) {
  if (reason) cerr << "Error: " << reason << "\n";
  cerr // TODO: better/proper (+ eg. man page)
    << "Usage: " << prog << " [-Dc] <script...>|<file>\n"
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

// `inc <filename>` directive?
string const prelude_source = R"(
  def lines [split:\n:];
  def unlines [join:\n:];
)";

void build(App& app, char const* const srcs[]) {
  stringstream source;
  source << prelude_source;
  while (*srcs) source << *srcs++ << ' ';

  try { source >> app; }

  catch (ParseError const& err) {
    cerr
      << "Error: (building application) "
      << err.what() << '\n'
      << "at: " << source.str().substr(prelude_source.length()) << '\n'
      << "    " << string(err.start-prelude_source.length(), ' ') << string(err.span, '~') << '\n'
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

int main(int argc, char const* const argv[]) {
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
  bool was_file = false;
  if (!argv[1]) {
    // boooring
    std::ifstream file(argv[0], std::ios::binary | std::ios::ate);
    if (file.is_open()) {
      std::streamsize size = file.tellg();
      file.seekg(0, std::ios::beg);
      std::vector<char> buffer(size);
      if (file.read(buffer.data(), size)) {
        was_file = true;
        char const* const srcs[2] = {buffer.data(), NULL};
        build(app, srcs);
      }
    }
  }
  if (!was_file) build(app, argv);

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
