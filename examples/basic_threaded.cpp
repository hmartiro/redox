/**
*
*/

#include <iostream>
#include <chrono>
#include <thread>
#include "../src/redox.hpp"

using namespace std;

redox::Redox rdx = {"localhost", 6379};

int main(int argc, char* argv[]) {

  rdx.start();

  thread setter([]() {
    for(int i = 0; i < 5000; i++) {
      rdx.command<int>("INCR counter");
      this_thread::sleep_for(chrono::milliseconds(1));
    }
    cout << "Setter thread exiting." << endl;
  });

  thread getter([]() {
    for(int i = 0; i < 5; i++) {
      rdx.command<string>(
          "GET counter",
          [](const string& cmd, const string& value) {
            cout << cmd << ": " << value << endl;
          }
      );
      this_thread::sleep_for(chrono::milliseconds(1000));
    }
    cout << "Getter thread exiting." << endl;
  });

  setter.join();
  getter.join();

  rdx.stop();

  return 0;
};
