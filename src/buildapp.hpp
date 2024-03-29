#ifndef SELI_BUILDAPP_HPP
#define SELI_BUILDAPP_HPP

#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

#include "sel/application.hpp"
#include "sel/errors.hpp"
#include "sel/parser.hpp"

using namespace std;
using namespace sel;

// `inc <filename>` directive?
string const prelude_source = /**/"";/*/ R"(
  def lines:: [split:\n:];
  def unlines:: [join:\n:];
)"; //*/

int build(App& app, char const* const srcs[]) {
  stringstream source;
  source << prelude_source;
  while (*srcs) source << *srcs++ << ' ';

  try { source >> app; }

  catch (ParseError const& err) {
    // YYY: 'cause, like, this should not happen..
    // (this way of adding the prelude should be temporary anyway)
    if (err.start < prelude_source.length()) {
      cerr << "This is an error in the hard-coded prelude!\n";
      return -1;
    }

    size_t at = err.start - prelude_source.length();
    bool has_pline = false;
    string pline;
    string line = source.str().substr(prelude_source.length());

    size_t count = 1;
    string::size_type ln;
    while (string::npos != (ln = line.find('\n'))) {
      if (at < ln+1) {
        line = line.substr(0, ln);
        break;
      }
      count++;
      at-= ln+1;
      has_pline = true;
      pline = line.substr(0, ln);
      line = line.substr(ln+1);
    }

    cerr
      << "Parsing error: "
      << err.what() << '\n'
    ;
    if (has_pline) cerr
      << setw(5) << count-1 << " | " << pline << '\n'
    ;
    cerr
      << setw(5) << count << " | " << line << '\n'
      << setw(5) << ' ' << " ` " << string(at, ' ') << string(err.span, '~') << '\n'
    ;
    return EXIT_FAILURE;
  }

  catch (TypeError const& err) {
    cerr
      << "Type error: "
      << err.what() << '\n'
      // << "at: " << source.str().substr(prelude_source.length()) << '\n'
      // << "    " << string(err.start-prelude_source.length(), ' ') << string(err.span, '~') << '\n'
    ;
    return EXIT_FAILURE;
  }

  catch (BaseError const& err) {
    cerr
      << "Error (while building application): "
      << err.what() << '\n'
    ;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

int buildfile(App& app, char const* filename) {
  ifstream file(filename, ios::binary | ios::ate);

  if (file.is_open()) {
    streamsize size = file.tellg();
    file.seekg(0, ios::beg);
    vector<char> buffer(size);

    if (file.read(buffer.data(), size)) {
      char const* const srcs[2] = {buffer.data(), NULL};
      return build(app, srcs);

    } else {
      cerr << "Could not read file: " << utils::quoted(filename) << "\n";
      return EXIT_FAILURE;
    }

    // unreachable
    return EXIT_SUCCESS;
  }

  // maybe it's a pipe or such (can't open "at end") -- try again
  ifstream notfile(filename, ios::binary);

  if (notfile) {
    string buffer;
    char small[4096];
    while (notfile.read(small, sizeof(small)))
      buffer+= small;
    auto end = notfile.gcount();
    small[end] = '\0';
    buffer+= small;

    char const* const srcs[2] = {buffer.data(), NULL};
    return build(app, srcs);

  } else {
    cerr << "Could not open file: " << utils::quoted(filename) << "\n";
    return EXIT_FAILURE;
  }

  // unreachable
  return EXIT_SUCCESS;
}

#endif // SELI_BUILDAPP_HPP
