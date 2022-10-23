#include <iostream>
#include <fstream>
#include <string>

// #include "sel/types.hpp"

using namespace std;
// using namespace sel;

string prog;

string itxt;
string ohpp;
string ocpp;
string ovst;

string name;
string upname;

void usage(char const* fail) {
  cerr << "Usage: " << prog << " <xyz.txt> <xyz.hpp> <xyz.cpp> <xyz_visit_each>\n";
  cerr << "failed because: " << fail << endl;
  exit(1);
}

string basename(string const& file) {
  return file.substr(file.find_last_of("/\\")+1);
}

int main(int argc, char* argv[]) {
  prog = argv[0];
  if (argc < 5) usage("not enough arguments");

  itxt = argv[1];
  ohpp = argv[2];
  ocpp = argv[3];
  ovst = argv[4];

  string bn = basename(itxt);
  name = bn.substr(0, bn.length()-4);

  if (name + ".txt" != basename(itxt)) usage("names of the '.txt' file does not match");
  if (name + ".hpp" != basename(ohpp)) usage("names of the '.hpp' file does not match");
  if (name + ".cpp" != basename(ocpp)) usage("names of the '.cpp' file does not match");
  if (name + "_visit_each" != basename(ovst)) usage("names of the '_visit_each' file does not match");

  upname = name;
  for (auto& c : upname)
    if ('a' <= c && c <= 'z')
      c = c+'C'-'c';
    else usage("name contains characters outside [a-z]");

  // ---

  ofstream ofhpp(ohpp);
  ofhpp
    << "#ifndef SEL_" << upname << "_HPP\n"
    << "#define SEL_" << upname << "_HPP\n"
    << "\n"
    << "namespace sel {\n"
    << "} // namespace sel\n"
    << "\n"
    << "#endif // SEL_" << upname << "_HPP"
    << endl;
  ofhpp.close();

  ofstream ofcpp(ocpp);
  ofcpp
    << "#include \"" << name << ".hpp\"\n"
    << "\n"
    << "namespace sel {\n"
    << "} // namespace sel"
    << endl;
  ofcpp.close();

  ofstream ofvst(ovst);
  ofvst
    << "#define " << upname << "_VISIT_EACH(__do)"
    << endl;
  ofvst.close();

  return EXIT_SUCCESS;
}
