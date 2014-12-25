/**
* Basic asynchronous calls using redisx.
*/

#include <iostream>
#include "../src/redisx.hpp"

using namespace std;

double time_s() {
  unsigned long ms = chrono::system_clock::now().time_since_epoch() / chrono::microseconds(1);
  return (double)ms / 1e6;
}

int main(int argc, char* argv[]) {

  redisx::Redis rdx = {"localhost", 6379};
  rdx.run();

  string cmd_str = "SET alaska rules!";

  double freq = 10000; // Hz
  double dt = 1 / freq; // s
  double t = 5; // s

  cout << "Running \"" << cmd_str << "\" at dt = " << dt
      << "s for " << t << "s..." << endl;

  int count = 0;
  redisx::Command<const string&>* c = rdx.command<const string&>(
      cmd_str,
      [&count](const string &cmd, const string &value) {
        count++;
      },
      dt,
      dt
  );

  double t0 = time_s();
  this_thread::sleep_for(chrono::microseconds((int)(t*1e6)));
  rdx.cancel<const string&>(c);
  rdx.stop();

  double t_elapsed = time_s() - t0;
  double actual_freq = (double)count / t_elapsed;

  cout << "Sent " << count << " commands in " << t_elapsed << "s, "
       << "that's " << actual_freq << " commands/s." << endl;

  return 0;
}
