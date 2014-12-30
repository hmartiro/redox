/**
* Basic use of Redox to set and get a Redis key.
*/

#include <iostream>
#include "../src/redox.hpp"

using namespace std;

int main(int argc, char* argv[]) {

  redox::Redox rdx = {"localhost", 6379};
  rdx.run();

  if(!rdx.set("alaska", "rules"))
    cerr << "Failed to set key!" << endl;

  cout << "alaska: " << rdx.get("alaska") << endl;

  rdx.stop();
}
