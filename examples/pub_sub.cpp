/**
* Redox example for basic pubsub usage.
*/

#include <stdlib.h>
#include <iostream>
#include "../src/redox.hpp"
#include "../src/subscriber.hpp"

using namespace std;

int main(int argc, char *argv[]) {

  redox::Redox publisher; // Initialize Redox (default host/port)
  if (!publisher.connect()) return 1; // Start the event loop

  redox::Subscriber subscriber;
  if(!subscriber.connect()) return 1;

  auto got_message = [](const string& topic, const string& msg) {
    cout << topic << ": " << msg << endl;
  };

  auto subscribed = [](const string& topic) {
    cout << "> Subscribed to " << topic << endl;
  };

  auto unsubscribed = [](const string& topic) {
    cout << "> Unsubscribed from " << topic << endl;
  };

  subscriber.psubscribe("news", got_message, subscribed, unsubscribed);
  subscriber.subscribe("sports", got_message, subscribed, unsubscribed);

  this_thread::sleep_for(chrono::milliseconds(10));

  publisher.publish("news", "one");
  publisher.publish("news", "two", [](const string& topic, const string& msg) {
    cout << "published to " << topic << ": " << msg << endl;
  });
  publisher.publish("sports", "three");

  this_thread::sleep_for(chrono::milliseconds(10));
  subscriber.unsubscribe("sports");
  publisher.publish("sports", "\"UH OH\"");
  publisher.publish("news", "four");

  this_thread::sleep_for(chrono::milliseconds(10));
  subscriber.punsubscribe("news");
  this_thread::sleep_for(chrono::milliseconds(10));

  publisher.publish("sports", "\"UH OH\"");
  publisher.publish("news", "\"UH OH\"");

  this_thread::sleep_for(chrono::milliseconds(10));

  return 0;
}
