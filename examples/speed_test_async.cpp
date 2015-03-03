/**
* Redox test
* ----------
* Increment a key on Redis using an asynchronous command on a timer.
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

  Redox rdx;
  rdx.noWait(true);

  if(!rdx.connect()) return 1;

  if(rdx.set("simple_loop:count", "0")) {
    cout << "Reset the counter to zero." << endl;
  } else {
    cerr << "Failed to reset counter." << endl;
    return 1;
  }

  vector<string> cmd_vec = {"INCR", "simple_loop:count"};
  double freq = 400000; // Hz
  double dt = 1 / freq; // s
  double t = 5; // s

  cout << "Sending \"" << rdx.vecToStr(cmd_vec) << "\" asynchronously every "
       << dt << "s for " << t << "s..." << endl;

  double t0 = time_s();
  atomic_int count(0);

  Command<int>& cmd = rdx.commandLoop<int>(
      cmd_vec,
      [&count, &rdx](Command<int>& c) {
        if (!c.ok()) {
          cerr << "Bad reply: " << c.status() << endl;
        }
        count++;
      },
      dt
  );

  // Wait for t time, then stop the command.
  this_thread::sleep_for(chrono::microseconds((int)(t*1e6)));
  cmd.free();

  long final_count = stol(rdx.get("simple_loop:count"));

  double t_elapsed = time_s() - t0;
  double actual_freq = (double)count / t_elapsed;

  cout << "Sent " << count << " commands in " << t_elapsed << "s, "
      << "that's " << actual_freq << " commands/s." << endl;

  cout << "Final value of counter: " << final_count << endl;

  rdx.disconnect();
  return 0;
};
