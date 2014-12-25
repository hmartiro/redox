/**
* Basic asynchronous calls using redisx.
*/

#include <iostream>
#include <thread>
#include <chrono>
#include "../src/redisx.hpp"

using namespace std;

redisx::Redis rdx = {"localhost", 6379};

int main(int argc, char* argv[]) {

  rdx.run();

  thread setter([]() {
    while(true) {
      rdx.command("INCR counter");
      this_thread::sleep_for(chrono::milliseconds(1));
    }
  });

  thread getter([]() {
    while(true) {
      rdx.command<const string &>(
          "GET counter",
          [](const string& cmd, const string& value) {
            cout << cmd << ": " << value << endl;
          }
      );
      this_thread::sleep_for(chrono::milliseconds(1000));
    }
  });

  setter.join();
  getter.join();

  return 0;
};
