/*
* Test for analyzing the jitter of commands.
*/

#include <iostream>
#include <string.h>
#include "redox.hpp"

using namespace std;
using redox::Redox;
using redox::Command;

double time_s() {
  unsigned long ms = chrono::system_clock::now().time_since_epoch() / chrono::microseconds(1);
  return (double)ms / 1e6;
}

int main(int argc, char* argv[]) {

  string usage_string = "Usage: " + string(argv[0]) + " --(set-async|get-async|set-sync|get-sync)";
  if(argc != 2) {
    cerr << usage_string<< endl;
    return 1;
  }

  Redox rdx;
  if(!rdx.connect("localhost", 6379)) return 1;

  double freq = 1000; // Hz
  double dt = 1 / freq; // s
  int iter = 1000000;
  atomic_int count(0);

  double t = time_s();
  double t_new = t;

  double t_reply = t;
  double t_reply_new = t;

  if(!strcmp(argv[1], "--get-async")) {

    while(count < iter) {
      rdx.command<string>({"GET", "jitter_test:time"},
          [&](Command<string>& c) {
            if (!c.ok()) {
              cerr << "Bad reply: " << c.status() << endl;
            } else {
              t_new = time_s();
              t_reply_new = stod(c.reply());
              cout << "dt real: " << (t_new - t) * 1000
                  << ", dt msg: " << (t_reply_new - t_reply) * 1000 << endl;
              t = t_new;
              t_reply = t_reply_new;
            }
            count++;
            if (count == iter) rdx.stop();
          }
      );

      this_thread::sleep_for(chrono::microseconds((int)((dt) * 1e6)));
    }

  } else if(!strcmp(argv[1], "--get-async-loop")) {

      rdx.commandLoop<string>({"GET", "jitter_test:time"},
          [&](Command<string>& c) {
            if (!c.ok()) {
              cerr << "Bad reply: " << c.status() << endl;
            } else {
              t_new = time_s();
              t_reply_new = stod(c.reply());
              cout << "dt real: " << (t_new - t) * 1000
                  << ", dt msg: " << (t_reply_new - t_reply) * 1000 << endl;
              t = t_new;
              t_reply = t_reply_new;
            }
            count++;
            if (count == iter) rdx.stop();
          },
          .001
      );

  } else if(!strcmp(argv[1], "--set-async")) {

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

    while(count < iter) {
      Command<string>& c = rdx.commandSync<string>({"GET", "jitter_test:time"});
      if(!c.ok()) {
        cerr << "Error setting value: " << c.status() << endl;
      } else {
        t_new = time_s();
        t_reply_new = stod(c.reply());
        cout << "dt real: " << (t_new - t) * 1000
            << ", dt msg: " << (t_reply_new - t_reply) * 1000 << endl;
        t = t_new;
        t_reply = t_reply_new;
      }
      count++;
      if(count == iter) rdx.stop();
      c.free();
      this_thread::sleep_for(chrono::microseconds((int)((dt) * 1e6)));
    }

  } else if(!strcmp(argv[1], "--set-sync")) {

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
  } else {
    cerr << usage_string << endl;
    return 1;
  }

  rdx.wait();
  return 0;
};
