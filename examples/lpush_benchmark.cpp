/**
* Basic asynchronous calls using redisx.
*/

#include <iostream>
#include "../src/redisx.hpp"

using namespace std;

unsigned long time_ms() {
  return chrono::system_clock::now().time_since_epoch()
      /chrono::milliseconds(1);
}

int main(int argc, char* argv[]) {

  redisx::Redis rdx = {"localhost", 6379};
  rdx.run();

  // TODO wait for this somehow
  rdx.command("DEL test");

  unsigned long t0 = time_ms();
  unsigned long t1 = t0;

  int len = 1000000;
  int count = 0;

  for(int i = 1; i <= len; i++) {
    rdx.command<int>("lpush test 1", [&t0, &t1, &count, len, &rdx](const string& cmd, int reply) {

      count++;
      if(count == len) {
        cout << cmd << ": " << reply << endl;

        unsigned long t2 = time_ms();
        cout << "Time to queue async commands: " << t1 - t0 << "ms" << endl;
        cout << "Time to receive all: " << t2 - t1 << "ms" <<  endl;
        cout << "Total time: " << t2 - t0 << "ms" <<  endl;

        rdx.stop();
      }
    });
  }
  t1 = time_ms();

  rdx.block_until_stopped();

  cout << "Commands processed: " << rdx.num_commands_processed() << endl;

  return 0;
};
