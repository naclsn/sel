#ifndef SELI_OPTIONS_HPP
#define SELI_OPTIONS_HPP

#include <iostream>
#include <sstream>
#include <string>

#include "sel/utils.hpp"

using namespace std;
using namespace sel;

// TODO: rewrite, but less "C" (eg. gotos)
struct Options {
  int argc;
  char** argv;
  char const* prog;

  bool exit = false; // true when eg. unsuccessful parse
  int exit_code;

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
    if (!argc) { usage(NULL); return; }

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
              return;

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
              else { usage("missing file name"); return; }
              // check file readable here?
              goto break_one;

            default:
              usage((string("unknown flag '")+*arg+'\'').c_str());
              return;
          } // switch *arg
        }
        break_one:;

      } else { // startswith '--'
        string argpp = arg;

        if ("--" == argpp) {
          script = argv+k+1;
          break;

        } else if ("--help" == argpp) {
          usage(NULL);
          return;

        } else if ("--version" == argpp) {
#define xtocstr(x) tocstr(x)
#define tocstr(x) #x
          cout << xtocstr(SEL_VERSION) << endl;
          exit = true;
          exit_code = EXIT_SUCCESS;
          return;
#undef tocstr
#undef xtocstr

        } else {
          ostringstream oss("unknown long argument: ", ios::ate);
          oss << utils::quoted(argpp);
          usage(oss.str().c_str());
          return;
        } // ifs argpp

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
    exit = true;
    exit_code = EXIT_FAILURE;
  }

  void verify() {
    if (!lookup && !script && !filename) usage("missing script");
  }
};

#endif // SELI_OPTIONS_HPP
