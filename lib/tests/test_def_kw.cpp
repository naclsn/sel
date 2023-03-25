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

  istringstream zin(given);
  ostringstream zout;
  app.run(zin, zout);
  string got = zout.str();

  cout << "--- given:\n" << given << "\n";
  cout << "--- got:\n" << got << "\n";
  cout << "---\n" << "\n";

  assert_cmp(expect, got);
  return 0;
}

TEST(def_kw) {
  return
  do_run({
    "split", ":", ":,",
      "map", "tonum,",
        "def", "incall:",
          "increment", "every", "number", "in", "the", "list:",
          "[map", "+1],",
        "incall,",
        "incall,",
        "incall,",
      "map", "tostr,",
    "join", ":", ":",
  }, "1 2 3", "5 6 7")+
  do_run({
    "def", "mylines::", "[split", ":\\n:];\n",
    "def", "myunlines::", "[join", ":\\n:];\n",
    "def", "ltonum::", "[map", "tonum];\n",
    "def", "ltostr::", "[map", "tostr];\n",
    "def", "mappp::", "[map", "+1];\n",
    "mylines,", "ltonum,", "mappp,", "ltostr,", "myunlines,", "ln",
  }, "1\na\n2\nb\n3\nc", "2\n1\n3\n1\n4\n1\n")+
  0;
}
