#include "common.hpp"

void build(App& app, vector<char const*> srcs) {
  stringstream source;
  for (auto const& it : srcs)
    source << it << ' ';
  source >> app;
}

int do_run(initializer_list<char const*> script_args, string given, string expect) {
  App app;
  build(app, script_args);
  cout << "[=\n";
  app.repr(cout);
  cout << "=]\n";

  istringstream zin(given);
  ostringstream zout;
  app.run(zin, zout);
  string got = zout.str();

  cout << "--- given:\n" << given << "\n";
  cout << "--- got:\n" << got << "\n";
  cout << "---\n";

  assert_cmp(expect, got);
  return 0;
}

TEST(run) {
  return
  // tonum, add 1, tostr
  do_run({"tonum,", "add", "1,", "tostr"}, "42\n", "43")+
  // split :_:, map [tonum, -1, tostr], join :#:
  do_run({"split", ":_:,", "map", "[tonum,", "-1,", "tostr],", "join", ":#:"}, "42_37", "41#36")+
  // split :-:, take 5, join :-:
  do_run({"split", ":-:,", "take", "5,", "join", ":-:"}, "a-b-c-d-e-f-g-h-i-j", "a-b-c-d-e")+
  // const 1, iterate +1, filter -5, map [tostr, ln], take 10, join ::
  do_run({"const", "1,", "iterate", "+1,", "filter", "-5,", "map", "[tostr,", "ln],", "take", "10,", "join", "::"}, "", "1\n2\n3\n4\n6\n7\n8\n9\n10\n11\n")+
  0;
}
