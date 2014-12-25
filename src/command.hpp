/**
* Redis C++11 wrapper.
*/

#pragma once

#include <iostream>
#include <string>
#include <functional>

#include <hiredis/adapters/libev.h>
#include <hiredis/async.h>

namespace redisx {

template<class ReplyT>
class Command {
public:
  Command(
      redisAsyncContext* c,
      const std::string& cmd,
      const std::function<void(const std::string&, ReplyT)>& callback,
      double repeat, double after
  );

  redisAsyncContext* c;
  const std::string cmd;
  const std::function<void(const std::string&, ReplyT)> callback;
  double repeat;
  double after;
  bool done;
  ev_timer* timer;
  std::mutex timer_guard;

  void invoke(ReplyT reply);

  void free_if_done();
  ev_timer* get_timer() {
    std::lock_guard<std::mutex> lg(timer_guard);
    return timer;
  }
};

template<class ReplyT>
Command<ReplyT>::Command(
    redisAsyncContext* c,
    const std::string& cmd,
    const std::function<void(const std::string&, ReplyT)>& callback,
    double repeat, double after
) : c(c), cmd(cmd), callback(callback), repeat(repeat), after(after), done(false) {
  timer_guard.lock();
}

template<class ReplyT>
void Command<ReplyT>::invoke(ReplyT reply) {
  if(callback != NULL) callback(cmd, reply);
  if((repeat == 0)) done = true;
}

template<class ReplyT>
void Command<ReplyT>::free_if_done() {
  if(done) {
    std::cout << "Deleting Command: " << cmd << std::endl;
    delete this;
  };
}
} // End namespace redis
