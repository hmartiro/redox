/**
* Basic asynchronous calls using redisx.
*/

#include <iostream>
#include "../src/redisx.hpp"

using namespace std;

int main(int argc, char* argv[]) {

  redisx::Redis rdx = {"localhost", 6379};

  rdx.command<const string &>("SET alaska rules!", [](const string &cmd, const string &value) {
    cout << cmd << ": " << value << endl;
  });

  rdx.command<const string &>("GET alaska", [](const string &cmd, const string &value) {
    cout << cmd << ": " << value << endl;
  });

  rdx.run_blocking();
}
