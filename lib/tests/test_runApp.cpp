#include "common.hpp"

void build(App& app, vector<char const*> srcs) {
  stringstream source;
  for (auto const& it : srcs)
    source << it << ' ';
  source >> app;
}

void do_run(initializer_list<char const*> script_args, string given, string expect) {
  App app;
  build(app, script_args);

  istringstream zin(given);
  ostringstream zout;
  app.run(zin, zout);
  string got = zout.str();

  cout << "--- given:\n" << given;
  cout << "--- got:\n" << got;
  cout << "---\n";

  assert_cmp(expect, got);
}

TEST(run) {
  do_run({"tonum," "add 1," "tostr"}, "42\n", "43");
  do_run({"split :_:,", "map", "[tonum,", "-1,", "tostr],", "join", ":#:"}, "42_37", "41#36");
}
