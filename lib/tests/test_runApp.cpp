#include "common.hpp"

void build(App& app, vector<char const*> srcs) {
  stringstream source;
  for (auto const& it : srcs)
    source << it << ' ';
  source >> app;
}

// class my_sb : public basic_streambuf<char> {
//   basic_streambuf<char>& under;
//   streamsize limit, current;
//   void (*then)();
// public:
//   my_sb(basic_streambuf<char>& under, streamsize limit, void (*then)())
//     : under(under)
//     , limit(limit)
//     , current(0)
//     , then(then)
//   { }
//   my_sb(basic_streambuf<char>& under, streamsize limit): my_sb(under, limit, NULL) { }
//   streamsize xsputn(const char* s, streamsize n) override {
//     if (limit < current + n && (n = under.sputn(s, limit-current)) == limit-current) then();
//     else n = under.sputn(s, n);
//     current+= n;
//     return n;
//   }
//   int sync() override { return under.pubsync(); }
// };

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
  do_run({"split", ":_:,", "map", "[tonum,", "-1,", "tostr],", "join", ":#:"}, "42_37", "41#36");
  // XXX: it it not possible to test script that would never finish
  //      idealy it would be possible to simulate a `| head`
  //do_run({"const", "1,", "iterate", "+1,", "filter", "-5,", "map", "[tostr,", "nl],", "join", "::"}, "", "");
}
