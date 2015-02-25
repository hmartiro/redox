/**
* Basic asynchronous calls using redox.
*/

#include <iostream>
#include "redox.hpp"

using namespace std;
using redox::Redox;
using redox::Command;

double time_s() {
  unsigned long ms = chrono::system_clock::now().time_since_epoch() / chrono::microseconds(1);
  return (double)ms / 1e6;
}

int main(int argc, char* argv[]) {

  redox::Redox rdx;

  if(!rdx.connect()) return 1;

  rdx.del("test");

  double t0 = time_s();
  double t1 = t0;

  int len = 1000000;
  atomic_int count = {0};

  for(int i = 1; i <= len; i++) {
    rdx.command<int>({"lpush", "test", "1"}, [&t0, &t1, &count, len, &rdx](Command<int>& c) {

      if(!c.ok()) return;

      count += 1;

      if(count == len) {
        cout << c.cmd() << ": " << c.reply() << endl;

        double t2 = time_s();
        cout << "Time to queue async commands: " << t1 - t0 << "s" << endl;
        cout << "Time to receive all: " << t2 - t1 << "s" <<  endl;
        cout << "Total time: " << t2 - t0 << "s" <<  endl;
        cout << "Result: " << (double)len / (t2-t0) << " commands/s" << endl;

        rdx.stop();
      }
    });
  }
  t1 = time_s();

  rdx.wait();
  return 0;
};
