/**
* Basic use of Redox to set and get binary data.
*/

#include <iostream>
#include <algorithm>
#include <random>
#include "../src/redox.hpp"

using namespace std;
using redox::Redox;
using redox::Command;

/**
* Random string generator.
*/
std::string random_string(size_t length) {
  std::string str(length, 0);
  std::generate_n(str.begin(), length, []{ return (unsigned char)rand(); });
  return str;
}

int main(int argc, char* argv[]) {

  redox::Redox rdx = {"localhost", 6379}; // Initialize Redox
  if(!rdx.start()) return 1; // Start the event loop

  rdx.del("binary");

  string binary_data = random_string(10000);

  auto& c = rdx.command_blocking<string>("SET binary \"" + binary_data + "\"");
  if(c.ok()) cout << "Reply: " << c.reply() << endl;
  else cerr << "Failed to set key! Status: " << c.status() << endl;
  c.free();

  auto& c2 = rdx.command_blocking<string>("GET binary");
  if(c2.ok()) {
    if(c2.reply() == binary_data) cout << "Binary data matches!" << endl;
    else cerr << "Binary data differs!" << endl;
  }
  else cerr << "Failed to get key! Status: " << c2.status() << endl;
  c2.free();

  rdx.stop(); // Shut down the event loop
  return 0;
}
