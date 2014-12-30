/**
* Redox test
* ----------
* Increment a key on Redis using synchronous commands in a loop.
*/

#include <iostream>
#include "../src/redox.hpp"

using namespace std;
using namespace redox;

double time_s() {
  unsigned long ms = chrono::system_clock::now().time_since_epoch() / chrono::microseconds(1);
  return (double)ms / 1e6;
}

int main(int argc, char* argv[]) {

  Redox rdx = {"localhost", 6379};
  rdx.start();

  if(rdx.command_blocking("SET simple_loop:count 0")) {
    cout << "Reset the counter to zero." << endl;
  } else {
    cerr << "Failed to reset counter." << endl;
    return 1;
  }

  string cmd_str = "INCR simple_loop:count";
  double t = 5; // s

  cout << "Sending \"" << cmd_str << "\" synchronously for " << t << "s..." << endl;

  double t0 = time_s();
  double t_end = t0 + t;
  int count = 0;

  while(time_s() < t_end) {
    Command<int>* c = rdx.command_blocking<int>(cmd_str);
    if(!c->ok()) cerr << "Bad reply, code: " << c->status() << endl;
    c->free();
    count++;
  }

  auto get_cmd = rdx.command_blocking<string>("GET simple_loop:count");
  long final_count = stol(get_cmd->reply());
  get_cmd->free();

  rdx.stop();

  double t_elapsed = time_s() - t0;
  double actual_freq = (double)count / t_elapsed;

  cout << "Sent " << count << " commands in " << t_elapsed << "s, "
       << "that's " << actual_freq << " commands/s." << endl;

  cout << "Final value of counter: " << final_count << endl;

  return 0;
}
