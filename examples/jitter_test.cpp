/*
* Test for analyzing the jitter of commands.
*/

#include <iostream>
#include <iomanip>
#include <string.h>
#include "redox.hpp"

using namespace std;
using redox::Redox;
using redox::Command;
using redox::Subscriber;

double time_s() {
  unsigned long ms = chrono::system_clock::now().time_since_epoch() / chrono::microseconds(1);
  return (double)ms / 1e6;
}

/**
* Prints time statistics on the received reply.
*
* t: Time since the program start.
* dt_callback: Time since the last reply was received.
* dt_msg: Time of the new message minus time of the last message.
* age_of_data: Time of message received minus time of message set.
*/
void print_time(double t, double dt_callback, double dt_msg, double age_of_data) {
  cout << "t: " << t * 1000
      << std::setiosflags(std::ios::fixed)
      << std::setprecision(3)
      << " | dt callback: " << dt_callback * 1000
      << " | dt msg: " << dt_msg * 1000
      << " | age of data: " << age_of_data * 1000 << endl;
}

int main(int argc, char* argv[]) {

  string usage_string = "Usage: " + string(argv[0])
      + " --(set-async|get-async|set-sync|get-sync|get-pubsub|set-pubsub) [freq]";

  if(argc != 3) {
    cerr << usage_string<< endl;
    return 1;
  }

  bool nowait = true;
  std::string host = "localhost";
  int port = 6379;

  Redox rdx;
  if(nowait) rdx.noWait(true);

  Subscriber rdx_sub;
  if(nowait) rdx_sub.noWait(true);

  double freq = stod(argv[2]); // Hz
  double dt = 1 / freq; // s
  int iter = 1000000;
  atomic_int count(0);

  double t0 = time_s();
  double t = t0;
  double t_new = t;

  double t_last_reply = t;
  double t_this_reply = t;

  if(!strcmp(argv[1], "--get-async")) {

    if(!rdx.connect(host, port)) return 1;

    while(count < iter) {
      rdx.command<string>({"GET", "jitter_test:time"},
          [&](Command<string>& c) {
            if (!c.ok()) {
              cerr << "Bad reply: " << c.status() << endl;
            } else {
              t_new = time_s();
              t_this_reply = stod(c.reply());
              print_time(
                  t_new - t0,
                  t_new - t,
                  t_this_reply - t_last_reply,
                  t_new - t_this_reply
              );
              t = t_new;
              t_last_reply = t_this_reply;
            }
            count++;
            if (count == iter) rdx.stop();
          }
      );

      this_thread::sleep_for(chrono::microseconds((int)(dt * 1e6)));
    }

  } else if(!strcmp(argv[1], "--get-async-loop")) {

    if(!rdx.connect(host, port)) return 1;

      rdx.commandLoop<string>({"GET", "jitter_test:time"},
          [&](Command<string>& c) {
            if (!c.ok()) {
              cerr << "Bad reply: " << c.status() << endl;
            } else {
              t_new = time_s();
              t_this_reply = stod(c.reply());
              print_time(
                  t_new - t0,
                  t_new - t,
                  t_this_reply - t_last_reply,
                  t_new - t_this_reply
              );
              t = t_new;
              t_last_reply = t_this_reply;
            }
            count++;
            if (count == iter) rdx.stop();
          },
          dt
      );

  } else if(!strcmp(argv[1], "--set-async")) {

    if(!rdx.connect(host, port)) return 1;

    while (count < iter) {
      rdx.command<string>({"SET", "jitter_test:time", to_string(time_s())},
          [&](Command<string>& c) {
            if (!c.ok()) {
              cerr << "Error setting value: " << c.status() << endl;
            }
            count++;
            if (count == iter) rdx.stop();
          }
      );
      this_thread::sleep_for(chrono::microseconds((int) (dt * 1e6)));
    }

  } else if(!strcmp(argv[1], "--get-sync")) {

    if(!rdx.connect(host, port)) return 1;

    while(count < iter) {
      Command<string>& c = rdx.commandSync<string>({"GET", "jitter_test:time"});
      if(!c.ok()) {
        cerr << "Error setting value: " << c.status() << endl;
      } else {
        t_new = time_s();
        t_this_reply = stod(c.reply());
        print_time(
            t_new - t0,
            t_new - t,
            t_this_reply - t_last_reply,
            t_new - t_this_reply
        );
        t = t_new;
        t_last_reply = t_this_reply;
      }
      count++;
      if(count == iter) rdx.stop();
      c.free();
      this_thread::sleep_for(chrono::microseconds((int)((dt) * 1e6)));
    }

  } else if(!strcmp(argv[1], "--set-sync")) {

    if(!rdx.connect(host, port)) return 1;

    while(count < iter){
      Command<string>& c = rdx.commandSync<string>({"SET", "jitter_test:time", to_string(time_s())});
      if(!c.ok()) {
        cerr << "Error setting value: " << c.status() << endl;
      }
      count++;
      if(count == iter) rdx.stop();
      c.free();
      this_thread::sleep_for(chrono::microseconds((int)((dt) * 1e6)));
    }

  } else if(!strcmp(argv[1], "--get-pubsub")) {

    if(!rdx_sub.connect(host, port)) return 1;

    auto got_message = [&](const string& topic, const string& msg) {

      t_new = time_s();
      t_this_reply = stod(msg);
      print_time(
          t_new - t0,
          t_new - t,
          t_this_reply - t_last_reply,
          t_new - t_this_reply
      );
      t = t_new;
      t_last_reply = t_this_reply;

      count++;
      if (count == iter) rdx.stop();
    };

    rdx_sub.subscribe("jitter_test:time", got_message);

  } else if(!strcmp(argv[1], "--set-pubsub")) {

    if(!rdx.connect(host, port)) return 1;

    while (count < iter) {
      double t1 = time_s();
      rdx.command<int>({"PUBLISH", "jitter_test:time", to_string(time_s())},
          [&](Command<int>& c) {
            if (!c.ok()) {
              cerr << "Error setting value: " << c.status() << endl;
            }
            count++;
            if (count == iter) rdx.stop();
          }
      );
      double wait = dt - (time_s() - t1);
      this_thread::sleep_for(chrono::microseconds((int) (wait * 1e6)));
    }

  } else {
    cerr << usage_string << endl;
    return 1;
  }

  rdx.wait();
  rdx_sub.wait();

  return 0;
};
