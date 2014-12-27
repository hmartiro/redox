/**
* Redis C++11 wrapper.
*/

#pragma once

#include <iostream>
#include <string>
#include <functional>
#include <atomic>

#include <hiredis/adapters/libev.h>
#include <hiredis/async.h>

namespace redisx {

template<class ReplyT>
class Command {

friend class Redis;

public:
  Command(
      redisAsyncContext* c,
      const std::string& cmd,
      const std::function<void(const std::string&, const ReplyT&)>& callback,
      const std::function<void(const std::string&, int status)>& error_callback,
      double repeat, double after
  );

  const std::string cmd;
  const double repeat;
  const double after;

  redisAsyncContext* c;

  std::atomic_int pending;

  void invoke(const ReplyT& reply);
  void invoke_error(int status);

private:

  const std::function<void(const std::string&, const ReplyT&)> callback;
  const std::function<void(const std::string&, int status)>& error_callback;

  std::atomic_bool completed;

  ev_timer* timer;
  std::mutex timer_guard;

  ev_timer* get_timer() {
    std::lock_guard<std::mutex> lg(timer_guard);
    return timer;
  }
};

template<class ReplyT>
Command<ReplyT>::Command(
    redisAsyncContext* c,
    const std::string& cmd,
    const std::function<void(const std::string&, const ReplyT&)>& callback,
    const std::function<void(const std::string&, int status)>& error_callback,
    double repeat, double after
) : cmd(cmd), repeat(repeat), after(after), c(c), pending(0),
    callback(callback), error_callback(error_callback), completed(false)
{
  timer_guard.lock();
}

template<class ReplyT>
void Command<ReplyT>::invoke(const ReplyT& reply) {

  if(callback != NULL) callback(cmd, reply);
  pending--;
  if((pending == 0) && (completed || (repeat == 0))) {
//    std::cout << "invoking success, reply: " << reply << std::endl;
//    std::cout << "Freeing cmd " << cmd << " in success invoke" << std::endl;
    delete this;
  }
}

template<class ReplyT>
void Command<ReplyT>::invoke_error(int status) {
  if(error_callback != NULL) error_callback(cmd, status);
  pending--;
  if((pending == 0) && (completed || (repeat == 0))) {
//    std::cout << "Freeing cmd " << cmd << " in error invoke" << std::endl;
    delete this;
  }
}

} // End namespace redis
