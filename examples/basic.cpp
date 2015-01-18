/**
* Basic use of Redox to set and get a Redis key.
*/

#include <iostream>
#include "../src/redox.hpp"

using namespace std;

int main(int argc, char* argv[]) {

  redox::Redox rdx = {"localhost", 6379}; // Initialize Redox
  if(!rdx.start()) return 1; // Start the event loop

  rdx.del("occupation");

  if(!rdx.set("occupation", "carpenter")) // Set a key, check if succeeded
    cerr << "Failed to set key!" << endl;

  cout << "key = occupation, value = \"" << rdx.get("occupation") << "\"" << endl;

  rdx.stop(); // Shut down the event loop
}
