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
  bool typecheck = false; // -n
  bool strict = false; // -s
  bool any_type = false; // -t (allows the app to be of any type)

  bool compile = false; // -o
  char** _compile_flags = NULL; // (everything after -o)
  // compile sub-command
  char const* outfile = NULL; // -o outfile (required)
  char const* funname = NULL; // -N funname (if set, no `main`)
  bool no_link = false; // -c (do not link)
  bool no_assemble = false; // -S (do not compile)

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
            case 'n': typecheck = true;   break;
            case 's': strict = true;      break;
            case 't': any_type = true;    break;

            case 'o':
              compile = true;
              if (*++arg) outfile = arg;
              else if (hasv) outfile = argv[++k];
              else usage("missing output file name");
              _compile_flags = argv+ ++k;
              goto break_all;

            case 'f':
              if (*++arg) filename = arg;
              else if (hasv) filename = argv[++k];
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

    if (compile) compile_subparser();

    verify();
  }

  void compile_subparser() {
    while (*_compile_flags) {
      char const* arg = *_compile_flags;

      if ('-' == arg[0]) {
        while (*++arg) {
          switch (*arg) {
            case 'N':
              if (*++arg) funname = arg;
              else if (*++_compile_flags) funname = *_compile_flags;
              else usage("missing function name");
              goto break_one;

            case 'c': no_link = true;     break;
            case 'S': no_assemble = true; break;

            default: usage((string("unknown flag '")+*arg+'\'').c_str());
          } // switch *arg
        }
        break_one:;

      } else usage((string("unexpected argument '")+arg+'\'').c_str());

      _compile_flags++;
    } // while compile_flags
  }

  void usage(char const* reason) {
    if (reason) cerr << "Error: " << reason << "\n";
    cerr
      << "Usage: " << prog << " [-Dnst] <script...> | -f <file>\n"
      << "       " << prog << " -l [<names...>]\n"
      << "       " << prog << " [-s] -f <file> [-o <bin> [-N <name> -cS]]\n"
    ;
    exit(EXIT_FAILURE);
  }

  void verify() {
    if (!lookup && !script && !filename) usage("missing script");
  }
};

#endif // SELI_OPTIONS_HPP
