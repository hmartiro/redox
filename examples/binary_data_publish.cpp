/**
* Basic use of Redox to publish and subscribe binary data.
*/

#include <iostream>
#include <algorithm>
#include <random>
#include <chrono>
#include "redox.hpp"

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

  redox::Redox rdx; // Initialize Redox
  redox::Subscriber sub; // Initialize Subscriber

  if(!rdx.connect("localhost", 6379)) return 1;
  if(!sub.connect("localhost", 6379)) return 1;

  string binary_key = random_string(100);
  string binary_data = random_string(10000);

  cout << "binary data size " << binary_data.length() << endl;

  sub.subscribe("test", [binary_data](const string& topic, const string& msg) {
    cout << "msg data size " << msg.length() << endl;
    if(msg == binary_data) cout << "Binary data matches!" << endl;
  });

  this_thread::sleep_for( chrono::milliseconds(1000) );

  rdx.publish("test", binary_data);

  this_thread::sleep_for( chrono::milliseconds(1000) );

  rdx.disconnect();
  sub.disconnect();
  return 0;
}
