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
//
//  Command<int>* del_cmd = rdx.command_blocking<int>("DEL simple_loop:count");
//  cout << "deleted key, reply: " << del_cmd->reply() << endl;
//  del_cmd->free();
  if(rdx.command_blocking("DEL simple_loop:count")) cout << "Deleted simple_loop:count" << endl;
  else cerr << "Failed to delete simple_loop:count" << endl;

  Command<char*>* set_cmd = rdx.command_blocking<char*>("SET simple_loop:count 0");
  cout << "set key, reply: " << set_cmd->reply() << endl;
  set_cmd->free();

  Command<char*>* count_cmd = rdx.command_blocking<char*>("GET simple_loop:count");
  if(count_cmd->status() == REDISX_OK) {
    cout << "At the start, simple_loop:count = " << count_cmd->reply() << endl;
  }
  count_cmd->free();

  string cmd_str = "INCR simple_loop:count";

  double freq = 10000; // Hz
  double dt = 1 / freq; // s
  double t = 1; // s

  cout << "Running \"" << cmd_str << "\" at dt = " << dt
      << "s for " << t << "s..." << endl;

  atomic_int count(0);
  Command<int>* c = rdx.command<int>(
      cmd_str,
      [&count](const string &cmd, const int& value) { count++; },
      NULL,
      dt,
      0
  );

  double t0 = time_s();
  this_thread::sleep_for(chrono::microseconds((int)(t*1e6)));
  rdx.cancel(c);

  cout << "At the end, simple_loop:count = "
       << rdx.command_blocking<string>("GET simple_loop:count")->reply() << endl;

  rdx.stop();

  double t_elapsed = time_s() - t0;
  double actual_freq = (double)count / t_elapsed;

  cout << "Sent " << count << " commands in " << t_elapsed << "s, "
       << "that's " << actual_freq << " commands/s." << endl;

  return 0;
}
