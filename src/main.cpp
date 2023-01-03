#include <iostream>
#include <sstream>
#include <fstream>

#include "sel/errors.hpp"
#include "sel/parser.hpp"

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

// `inc <filename>` directive?
string const prelude_source = /** /"";/*/ R"(
  def lines [split:\n:];
  def unlines [join:\n:];
)"; //*/

void build(App& app, char const* const srcs[]) {
  stringstream source;
  source << prelude_source;
  while (*srcs) source << *srcs++ << ' ';

  try { source >> app; }

  catch (ParseError const& err) {
    // YYY: 'cause, like, this should not happen..
    // (this way of adding the prelude should be temporary anyway)
    if (err.start < prelude_source.length()) {
      cerr << "This is an error in the hard-coded prelude!\n";
      exit(-1);
    }
    cerr
      << "Parsing error: "
      << err.what() << '\n'
      << "at: " << source.str().substr(prelude_source.length()) << '\n'
      << "    " << string(err.start-prelude_source.length(), ' ') << string(err.span, '~') << '\n'
    ;
    exit(EXIT_FAILURE);
  }

  catch (BaseError const& err) {
    cerr
      << "Error (while building application): "
      << err.what() << '\n'
    ;
    exit(EXIT_FAILURE);
  }
}

struct Options {
  int argc;
  char** argv;
  char const* prog;

  char** script = NULL; // script...
  char const* filename = NULL; // -f filename

  bool lookup = false; // -l
  char** lookup_names = NULL; // -l names...

  bool debug = false; // -D
  bool typecheck = false; // -c (niy)

  Options(int argc, char* argv[])
    : argc(argc--)
    , argv(argv)
    , prog(*argv++)
  {
    if (!argc) usage(NULL);

    for (int k = 0; k < argc; k++) {
      char const* arg = argv[k];
      if (!*arg) continue;

      if ('-' != arg[0]) {
        script = argv+k;
        break;

      } else if ('-' != arg[1]) { // startswith '-'
        bool hasv = k+1 < argc;

        while (*++arg) {
          switch (*arg) {
            case 'h':
              usage(NULL);
              break;

            case 'l':
              lookup = true;
              if (hasv) lookup_names = argv+ ++k;
              goto break_all;

            case 'D': debug = true;       break;
            case 'c': typecheck = true;   break;

            case 'f':
              if (hasv) filename = argv[++k];
              else usage("missing file name");
              // check file readable here?
              goto break_one;

            default: usage((string("unknown flag '")+*arg+'\'').c_str());
          } // switch *arg
        }
        break_one:;

      } else { // startswith '--'
        string argpp = arg;

        if ("--help" == argpp) {
          usage(NULL);

        } else if ("--version" == argpp) {
#define xtocstr(x) tocstr(x)
#define tocstr(x) #x
          cout << xtocstr(SEL_VERSION) << endl;
          exit(EXIT_SUCCESS);
#undef tocstr
#undef xtocstr

        } else {
          ostringstream oss("unknown long argument: ", ios::ate);
          oss << quoted(argpp);
          usage(oss.str().c_str());
        } // switch argpp

      } // if '-' / '--'
    } // for argv
    break_all:;

    verify();
  }

  void usage(char const* reason) {
    if (reason) cerr << "Error: " << reason << "\n";
    cerr // TODO: better/proper (+ eg. man page)
      << "Usage: " << prog << " [-Dc] <script...> | -f <file>\n"
      << "       " << prog << " -l [<names...>]\n"
    ;
    exit(EXIT_FAILURE);
  }

  void verify() {
    if (!lookup && !script && !filename) usage("missing script");
  }
};

int main(int argc, char* argv[]) {
  Options const opts(argc, argv);
  App app;

  if (opts.lookup) lookup(opts.lookup_names);

  if (opts.filename) {
    ifstream file(opts.filename, ios::binary | ios::ate);
    ostringstream errbuilder("could not ", ios::ate);
    if (file.is_open()) {
      // boooring
      streamsize size = file.tellg();
      file.seekg(0, ios::beg);
      vector<char> buffer(size);
      if (file.read(buffer.data(), size)) {
        char const* const srcs[2] = {buffer.data(), NULL};
        build(app, srcs);
      } else {
        cerr << "Could not read file: " << quoted(opts.filename) << "\n";
        exit(EXIT_FAILURE);
      }
    } else {
      cerr << "Could not open file: " << quoted(opts.filename) << "\n";
      exit(EXIT_FAILURE);
    }
  } else build(app, opts.script);

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
