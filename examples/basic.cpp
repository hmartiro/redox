/**
* Basic use of Redox to set and get a Redis key.
*/

#include <iostream>
#include "redox.hpp"

using namespace std;
using redox::Redox;
using redox::Command;

int main(int argc, char* argv[]) {

  Redox rdx = {"localhost", 6379, nullptr, cout, redox::log::Info}; // Initialize Redox

  if(!rdx.connect()) return 1; // Start the event loop

  rdx.del("occupation");

  if(!rdx.set("occupation", "carpenter")) // Set a key, check if succeeded
    cerr << "Failed to set key!" << endl;

  cout << "key = \"occupation\", value = \"" << rdx.get("occupation") << "\"" << endl;

  return 0;
}
