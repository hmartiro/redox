/**
* Redox example with multiple clients.
*/

#include <iostream>
#include "../src/redox.hpp"

using namespace std;
using redox::Redox;
using redox::Command;

int main(int argc, char* argv[]) {

  redox::Redox rdx1, rdx2, rdx3;

  if(!rdx1.start() || !rdx2.start() || !rdx3.start()) return 1;

  rdx1.del("occupation");

  if(!rdx2.set("occupation", "carpenter")) // Set a key, check if succeeded
    cerr << "Failed to set key!" << endl;

  cout << "key = occupation, value = \"" << rdx3.get("occupation") << "\"" << endl;

  rdx1.stop();
  rdx2.stop();
  rdx3.stop();

  return 0;
}
