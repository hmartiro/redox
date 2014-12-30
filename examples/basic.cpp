/**
* Basic use of Redox to set and get a Redis key.
*/

#include <iostream>
#include "../src/redox.hpp"

using namespace std;

int main(int argc, char* argv[]) {

  redox::Redox rdx; // Initialize Redox (default host/port)
  rdx.start(); // Start the event loop

  if(!rdx.set("alaska", "rules")) // Set a key, check if succeeded
    cerr << "Failed to set key!" << endl;

  cout << "key = alaska, value = " << rdx.get("alaska") << endl;

  rdx.stop(); // Shut down the event loop
}
