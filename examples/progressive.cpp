/**
* Basic asynchronous calls using redisx.
*/

#include <iostream>
#include "../src/redisx.hpp"

using namespace std;

redisx::Redis rdx = {"localhost", 6379};

void print_key(const string& key) {
  rdx.command<const string&>("GET " + key, [key](const string& cmd, const string& value) {
    cout << "[GET] " << key << ": \"" << value << '\"' << endl;
  });
}

void set_key(const string& key, const string& value) {
  string cmd_str = "SET " + key + " " + value;
  rdx.command<const string&>(cmd_str, [key, value](const string& cmd, const string& reply) {
    cout << "[SET] " << key << ": \"" << value << '\"' << endl;
  });
}

int main(int argc, char* argv[]) {

  set_key("name", "Bob");
  print_key("name");
  set_key("name", "Steve");
  print_key("name");

  rdx.run_blocking();
  return 0;
};
