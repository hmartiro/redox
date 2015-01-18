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

  Redox rdx;
  if(!rdx.start()) return 1;

  rdx.del("stringtest");
  rdx.set("stringtest", "value");

  int count = 1000000;
  double t0 = time_s();

  string cmd_str = "GET stringtest";
  for(int i = 0; i < count; i++) {
    rdx.command<string>(
      cmd_str,
      [](const string &cmd, string const& value) {
        value;
      },
      [](const string &cmd, int status) {
        cerr << "Bad reply: " << status << endl;
      }
    );
  }

  rdx.stop();

  double t_elapsed = time_s() - t0;

  cout << "Sent " << count << " commands in " << t_elapsed << "s." << endl;

  return 0;
}
