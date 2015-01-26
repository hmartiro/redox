#include <iostream>
#include "../src/redox.hpp"

using namespace std;
using redox::Redox;
using redox::Command;

double time_s() {
  unsigned long ms = chrono::system_clock::now().time_since_epoch() / chrono::microseconds(1);
  return (double)ms / 1e6;
}

int main(int argc, char *argv[]) {

  Redox rdx_pub;
  Redox rdx_sub;

  if(!rdx_pub.start()) return 1;
  if(!rdx_sub.start()) return 1;

  atomic_int count(0);
  auto got_message = [&count](const string& topic, const string& msg) {
    count += 1;
  };

  auto subscribed = [](const string& topic) {

  };

  auto unsubscribed = [](const string& topic) {
    cout << "> Unsubscribed from " << topic << endl;
  };

  rdx_sub.subscribe("speedtest", got_message, subscribed, unsubscribed);

  double t0 = time_s();
  double t1 = t0;
  double tspan = 5;

  while(t1 - t0 < tspan) {
    rdx_pub.publish("speedtest", "hello");
    t1 = time_s();
  }
  this_thread::sleep_for(chrono::milliseconds(1000));
  rdx_pub.stop();
  rdx_sub.stop();

  double t = t1 - t0;
  cout << "Total of messages sent in " << t << "s is " << count << endl;
  double msg_per_s = count / t;
  cout << "Messages per second: " << msg_per_s << endl;
}
