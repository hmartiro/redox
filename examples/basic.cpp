/**
* Basic use of Redox to set and get a Redis key.
*/

#include <iostream>
#include <redox.hpp>

using namespace std;
using redox::Redox;
using redox::Command;
using redox::Subscriber;

int main(int argc, char* argv[]) {

  Redox rdx;
  if(!rdx.connect("localhost", 6379)) return 1;

  rdx.del("occupation");

  if(!rdx.set("occupation", "carpenter")) // Set a key, check if succeeded
    cerr << "Failed to set key!" << endl;

  cout << "key = \"occupation\", value = \"" << rdx.get("occupation") << "\"" << endl;

  return 0;
}
