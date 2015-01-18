/**
* Basic use of Redox to set and get a Redis key.
*/

#include <iostream>
#include "../src/redox.hpp"

using namespace std;

int main(int argc, char* argv[]) {

  redox::Redox rdx; // Initialize Redox (default host/port)
  if(!rdx.start()) return 1; // Start the event loop

  rdx.del("mylist");

  rdx.command_blocking("LPUSH mylist 1 2 3 4 5 6 7 8 9 10");

  auto c = rdx.command_blocking<vector<string>>("LRANGE mylist -3 -1");
  if(!c->ok()) cerr << "Error with LRANGE: " << c->status() << endl;
  for(const string& s : c->reply()) cout << s << endl;
  c->free();

//  if(!rdx.set("apples", "are great!")) // Set a key, check if succeeded
//    cerr << "Failed to set key!" << endl;
//
//  cout << "key = alaska, value = \"" << rdx.get("apples") << "\"" << endl;

  rdx.stop(); // Shut down the event loop
}
