/**
* Basic use of Redox to set and get a Redis key.
*/

#include <iostream>
#include "../src/redox.hpp"

using namespace std;

int main(int argc, char* argv[]) {

  redox::Redox rdx = {"localhost", 6379};

  rdx.command<string>("SET alaska rules!", [](const string &cmd, const string &value) {
    cout << cmd << ": " << value << endl;
  });

  rdx.command<string>("GET alaska", [](const string &cmd, const string &value) {
    cout << cmd << ": " << value << endl;
  });

  rdx.run_blocking();
}
