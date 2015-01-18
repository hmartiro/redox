/**
* Redox test
* ----------
* Increment a key on Redis using an asynchronous command on a timer.
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
  if(!rdx.start()) return 1;

  if(rdx.command_blocking("SET simple_loop:count 0")) {
    cout << "Reset the counter to zero." << endl;
  } else {
    cerr << "Failed to reset counter." << endl;
    return 1;
  }

  string cmd_str = "INCR simple_loop:count";
  double freq = 400000; // Hz
  double dt = 1 / freq; // s
  double t = 5; // s

  cout << "Sending \"" << cmd_str << "\" asynchronously every "
       << dt << "s for " << t << "s..." << endl;

  double t0 = time_s();
  atomic_int count(0);

  Command<int>* c = rdx.command<int>(
      cmd_str,
      [&count, &rdx](const string &cmd, const int& value) { count++; },
      [](const string& cmd, int status) { cerr << "Bad reply: " << status << endl; },
      dt
  );

  // Wait for t time, then stop the command.
  this_thread::sleep_for(chrono::microseconds((int)(t*1e6)));
  c->cancel();

  // Get the final value of the counter
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
