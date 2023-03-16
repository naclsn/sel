#ifndef SELI_OPTIONS_HPP
#define SELI_OPTIONS_HPP

#include <iostream>
#include <sstream>
#include <string>

#include "sel/utils.hpp"

using namespace std;
using namespace sel;

struct Options {
  int argc;
  char** argv;
  char const* prog;

  char** script = NULL; // script...
  char const* filename = NULL; // -f filename

  bool lookup = false; // -l
  char** lookup_names = NULL; // -l names...

  bool debug = false; // -D (implies -t)
  bool any_type = false; // -t (allows the app to be of any type)
  bool typecheck = false; // -n
  bool strict = false; // -s

  bool compile = false; // -o (niy)
  char** compile_flags = NULL; // everything after -o (niy)

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

            case 'D': debug = true;       //break;
            case 't': any_type = true;    break;
            case 'n': typecheck = true;   break;
            case 's': strict = true;      break;

            case 'o':
              compile = true;
              if (hasv) compile_flags = argv+ ++k;
              goto break_all;

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
    cerr
      << "Usage: " << prog << " [-Dnst] <script...> | -f <file>\n"
      << "       " << prog << " -l [<names...>] | :: <type...>\n"
      << "       " << prog << " [-s] -f <file> [-o <bin> <flags...>]\n"
    ;
    exit(EXIT_FAILURE);
  }

  void verify() {
    if (!lookup && !script && !filename) usage("missing script");
  }
};

#endif // SELI_OPTIONS_HPP
