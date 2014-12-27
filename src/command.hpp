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

static const int REDISX_UNINIT = -1;
static const int REDISX_OK = 0;
static const int REDISX_SEND_ERROR = 1;
static const int REDISX_WRONG_TYPE = 2;
static const int REDISX_NIL_REPLY = 3;
static const int REDISX_ERROR_REPLY = 4;
static const int REDISX_TIMEOUT = 5;

template<class ReplyT>
class Command {

friend class Redis;

public:
  Command(
    redisAsyncContext* c,
    const std::string& cmd,
    const std::function<void(const std::string&, const ReplyT&)>& callback,
    const std::function<void(const std::string&, int status)>& error_callback,
    double repeat, double after,
    bool free_memory
  );

  const std::string cmd;
  const double repeat;
  const double after;

  const bool free_memory;

  redisAsyncContext* c;
  redisReply* reply_obj;

  std::atomic_int pending;

  void invoke(const ReplyT& reply);
  void invoke_error(int status);

  const ReplyT& reply();
  int status() { return reply_status; };

  /**
  * Called by the user to free the redisReply object, when the free_memory
  * flag is set to false for a command.
  */
  void free();

  void free_reply_object();

private:

  const std::function<void(const std::string&, const ReplyT&)> callback;
  const std::function<void(const std::string&, int status)> error_callback;

  // Place to store the reply value and status.
  // ONLY for blocking commands
  ReplyT reply_val;
  int reply_status;

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
    double repeat, double after, bool free_memory
) : cmd(cmd), repeat(repeat), after(after), free_memory(free_memory), c(c), reply_obj(NULL),
    pending(0), callback(callback), error_callback(error_callback), completed(false)
{
  timer_guard.lock();
}

template<class ReplyT>
void Command<ReplyT>::invoke(const ReplyT& r) {

  if(callback) callback(cmd, r);

  pending--;
  if(!free_memory) return;
  if(pending != 0) return;
  if(completed || (repeat == 0)) {
//    std::cout << cmd << ": suicide!" << std::endl;
    delete this;
  }
}

template<class ReplyT>
void Command<ReplyT>::invoke_error(int status) {

  if(error_callback) error_callback(cmd, status);

  pending--;
  if(!free_memory) return;
  if(pending != 0) return;
  if(completed || (repeat == 0)) {
//    std::cout << cmd << ": suicide!" << std::endl;
    delete this;
  }
}

template<class ReplyT>
void Command<ReplyT>::free_reply_object() {

  if(reply_obj == NULL) {
    std::cerr << "[ERROR] " << cmd << ": Attempting to double free reply object!" << std::endl;
    return;
  }

  freeReplyObject(reply_obj);
  reply_obj = NULL;
}

template<class ReplyT>
void Command<ReplyT>::free() {

  free_reply_object();

  // Commit suicide
//  std::cout << cmd << ": suicide, by calling free()!" << std::endl;
  delete this;
}

template<class ReplyT>
const ReplyT& Command<ReplyT>::reply() {
  if(reply_status != REDISX_OK) {
    std::cout << "[WARNING] " << cmd
              << ": Accessing value of reply with status != OK." << std::endl;
  }
  return reply_val;
}

} // End namespace redis
