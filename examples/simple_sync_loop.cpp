/**
* Basic asynchronous calls using redisx.
*/

#include <iostream>
#include "../src/redisx.hpp"

using namespace std;
using namespace redisx;

double time_s() {
  unsigned long ms = chrono::system_clock::now().time_since_epoch() / chrono::microseconds(1);
  return (double)ms / 1e6;
}

int main(int argc, char* argv[]) {

  Redis rdx = {"localhost", 6379};
  rdx.run();

  if(rdx.command_blocking("DEL simple_loop:count")) cout << "Deleted simple_loop:count" << endl;
  else cerr << "Failed to delete simple_loop:count" << endl;

  string cmd_str = "INCR simple_loop:count";

  int count = 50000;
  double t0 = time_s();

  cout << "Running \"" << cmd_str << "\" " << count << " times." << endl;

  for(int i = 0; i < count; i++) {
    Command<int>* c = rdx.command_blocking<int>(cmd_str);
    if(c->status() != REDIS_OK) cerr << "Bad reply, code: " << c->status() << endl;
  }

  cout << "At the end, simple_loop:count = "
    << rdx.command_blocking<string>("GET simple_loop:count")->reply() << endl;

  rdx.stop();

  double t_elapsed = time_s() - t0;
  double actual_freq = (double)count / t_elapsed;

  cout << "Sent " << count << " commands in " << t_elapsed << "s, "
    << "that's " << actual_freq << " commands/s." << endl;

  return 0;
}
