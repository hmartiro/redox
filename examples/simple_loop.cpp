/**
* Basic asynchronous calls using redisx.
*/

#include <iostream>
#include "../src/redisx.hpp"

using namespace std;

double time_s() {
  unsigned long ms = chrono::system_clock::now().time_since_epoch() / chrono::milliseconds(1);
  return (double)ms / 1000;
}

int main(int argc, char* argv[]) {

  redisx::Redis rdx = {"localhost", 6379};
  rdx.run();

  string cmd_str = "SET alaska rules!";

  double freq = 10000; // Hz
  double t_end = 5;

  double dt = 1 / freq;
  double t0 = time_s();
  int count = 0;

  cout << "Running \"" << cmd_str << "\" at dt = " << dt
      << "s for " << t_end << "s..." << endl;

  rdx.command<const string &>(
      cmd_str,
      [&count, &rdx, t0, t_end](const string &cmd, const string &value) {
        count++;
        if(time_s() - t0 >= t_end) rdx.stop();
      },
      dt,
      dt
  );

  rdx.block_until_stopped();
  double actual_freq = (double)count / t_end;
  cout << "Sent " << count << " commands in " << t_end<< "s, "
       << "that's " << actual_freq << " commands/s." << endl;

  return 0;
}
