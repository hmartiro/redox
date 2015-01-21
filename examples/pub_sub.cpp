#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "hiredis/hiredis.h"
#include "hiredis/async.h"
#include "hiredis/adapters/libev.h"
#include <iostream>
#include "../src/redox.hpp"

using namespace std;

int main(int argc, char *argv[]) {

  redox::Redox rdx; // Initialize Redox (default host/port)
  if (!rdx.start()) return 1; // Start the event loop

  auto got_message = [](const string& topic, const string& msg) {
    cout << topic << ": " << msg << endl;
  };

  auto subscribed = [](const string& topic) {
    cout << "> Subscribed to " << topic << endl;
  };

  auto unsubscribed = [](const string& topic) {
    cout << "> Unsubscribed from " << topic << endl;
  };

  rdx.subscribe("news", got_message, subscribed, unsubscribed);
  rdx.subscribe("sports", got_message, subscribed, unsubscribed);

  redox::Redox rdx_pub;
  if(!rdx_pub.start()) return 1;

  rdx_pub.publish("news", "hello!");
  rdx_pub.publish("news", "whatup");
  rdx_pub.publish("sports", "yo");

  this_thread::sleep_for(chrono::seconds(10));
  rdx.unsubscribe("sports");
  rdx_pub.publish("sports", "yo");
  rdx_pub.publish("news", "whatup");

  this_thread::sleep_for(chrono::seconds(10));
  rdx.unsubscribe("news");
  rdx_pub.publish("sports", "yo");
  rdx_pub.publish("news", "whatup", [](const string& topic, const string& msg) {
    cout << "published to " << topic << ": " << msg << endl;
  });

  rdx.block();
}
