/**
* Redis C++11 wrapper.
*/

#pragma once

#include <iostream>
#include <string>
#include <functional>
#include <atomic>
#include <mutex>

#include <hiredis/adapters/libev.h>
#include <hiredis/async.h>

namespace redox {

static const int REDOX_UNINIT = -1;
static const int REDOX_OK = 0;
static const int REDOX_SEND_ERROR = 1;
static const int REDOX_WRONG_TYPE = 2;
static const int REDOX_NIL_REPLY = 3;
static const int REDOX_ERROR_REPLY = 4;
static const int REDOX_TIMEOUT = 5;

template<class ReplyT>
class Command {

friend class Redox;

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

  static void command_callback(redisAsyncContext *c, void *r, void *privdata);

private:

  const std::function<void(const std::string&, const ReplyT&)> callback;
  const std::function<void(const std::string&, int status)> error_callback;

  // Place to store the reply value and status.
  // ONLY for blocking commands
  ReplyT reply_val;
  int reply_status;

  std::atomic_bool completed;

  ev_timer timer;
  std::mutex timer_guard;

  ev_timer* get_timer() {
    std::lock_guard<std::mutex> lg(timer_guard);
    return &timer;
  }

  void free_reply_object();

  void invoke_callback();
  bool is_error_reply();
  bool is_nil_reply();
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
void Command<ReplyT>::command_callback(redisAsyncContext *c, void *r, void *privdata) {

  auto *cmd_obj = (Command<ReplyT> *) privdata;
  cmd_obj->reply_obj = (redisReply *) r;
  cmd_obj->invoke_callback();

  // Free the reply object unless told not to
  if(cmd_obj->free_memory) cmd_obj->free_reply_object();
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
  if(reply_status != REDOX_OK) {
    std::cout << "[WARNING] " << cmd
              << ": Accessing value of reply with status != OK." << std::endl;
  }
  return reply_val;
}

} // End namespace redis
