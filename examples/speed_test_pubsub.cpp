#include <iostream>
#include "redox.hpp"

using namespace std;
using redox::Redox;
using redox::Command;
using redox::Subscriber;

double time_s() {
  unsigned long ms = chrono::system_clock::now().time_since_epoch() / chrono::microseconds(1);
  return (double)ms / 1e6;
}

int main(int argc, char *argv[]) {

  Redox rdx_pub;
  rdx_pub.noWait(true);

  Subscriber rdx_sub;
  rdx_sub.noWait(true);

  if(!rdx_pub.connect()) return 1;
  if(!rdx_sub.connect()) return 1;

  atomic_int count(0);
  auto got_message = [&count](const string& topic, const string& msg) {
    count += 1;
  };

  auto subscribed = [](const string& topic) {
    cout << "> Subscribed to " << topic << endl;
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

  this_thread::sleep_for(chrono::milliseconds(10));

  double t = t1 - t0;
  cout << "Total of messages sent in " << t << "s is " << count << endl;
  double msg_per_s = count / t;
  cout << "Messages per second: " << msg_per_s << endl;

  rdx_sub.disconnect();
  rdx_pub.disconnect();

  return 0;
}
