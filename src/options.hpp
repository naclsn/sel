#ifndef SELI_OPTIONS_HPP
#define SELI_OPTIONS_HPP

#include <string>
#include <iostream>
#include <sstream>

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

#endif // SELI_OPTIONS_HPP
