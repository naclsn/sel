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
  do_run({
    "split", ":", ":,",
      "map", "tonum,",
        "def", "incall", "[map", "+1],",
        "incall,",
        "incall,",
        "incall,",
      "map", "tostr,",
    "join", ":", ":,",
  }, "1 2 3", "5 6 7");
  do_run({
    "def", "lines", "[split", ":\n:];\n",
    "def", "unlines", "[join", ":\n:];\n",
    "def", "ltonum", "[map", "tonum];\n",
    "def", "ltostr", "[map", "tostr];\n",
    "def", "mappp", "[map", "+1];\n",
    "lines,", "ltonum,", "mappp,", "ltostr,", "unlines,", "nl",
  }, "1\na\n2\nb\n3\nc", "2\n1\n3\n1\n4\n1\n");
}
