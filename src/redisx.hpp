/**
* Redis C++11 wrapper.
*/

#pragma once

#include <iostream>
#include <functional>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include <string>
#include <queue>
#include <unordered_map>

#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libev.h>

#include "command.hpp"

static const int REDISX_UNINIT = -1;
static const int REDISX_OK = 0;
static const int REDISX_SEND_ERROR = 1;
static const int REDISX_WRONG_TYPE = 2;
static const int REDISX_NIL_REPLY = 3;
static const int REDISX_ERROR_REPLY = 4;
static const int REDISX_TIMEOUT = 5;

namespace redisx {

class Redis {

public:

  Redis(const std::string& host, const int port);
  ~Redis();

  void run();
  void run_blocking();
  void stop();
  void block();

  template<class ReplyT>
  Command<ReplyT>* command(
      const std::string& cmd,
      const std::function<void(const std::string&, const ReplyT&)>& callback = NULL,
      const std::function<void(const std::string&, int status)>& error_callback = NULL,
      double repeat = 0.0,
      double after = 0.0
  );

  template<class ReplyT>
  bool cancel(Command<ReplyT>* cmd_obj);

  void command(const std::string& command);

  template<class ReplyT>
  ReplyT command_blocking(const std::string& cmd);

  void command_blocking(const std::string& command);

  long num_commands_processed();

//  void get(const char* key, std::function<void(const std::string&, const char*)> callback);
//
//  void set(const char* key, const char* value);
//  void set(const char* key, const char* value, std::function<void(const std::string&, const char*)> callback);
//
//  void del(const char* key);
//  void del(const char* key, std::function<void(const std::string&, long long int)> callback);

//  void publish(std::string channel, std::string msg);
//  void subscribe(std::string channel, std::function<void(std::string channel, std::string msg)> callback);
//  void unsubscribe(std::string channel);

  // Map of ev_timer events to pointers to Command objects
  // Used to get the object back from the timer watcher callback
  static std::unordered_map<ev_timer*, void*> timer_callbacks;

private:

  // Redis server
  std::string host;
  int port;

  // Number of commands processed
  long cmd_count;

  redisAsyncContext *c;

  std::atomic_bool to_exit;
  std::mutex exit_waiter_lock;
  std::condition_variable exit_waiter;

  std::thread event_loop_thread;

  std::unordered_map<void*, Command<redisReply*>*> commands_redis_reply;
  std::unordered_map<void*, Command<std::string>*> commands_string_r;
  std::unordered_map<void*, Command<char*>*> commands_char_p;
  std::unordered_map<void*, Command<int>*> commands_int;
  std::unordered_map<void*, Command<long long int>*> commands_long_long_int;

  template<class ReplyT>
  std::unordered_map<void*, Command<ReplyT>*>& get_command_map();

  std::queue<void*> command_queue;
  std::mutex queue_guard;
  void process_queued_commands();

  template<class ReplyT>
  bool process_queued_command(void* cmd_ptr);

  template<class ReplyT>
  ReplyT copy_reply(const ReplyT& reply);
};

// ---------------------------

template<class ReplyT>
void invoke_callback(
    Command<ReplyT>* cmd_obj,
    redisReply* reply
);

template<class ReplyT>
void command_callback(redisAsyncContext *c, void *r, void *privdata) {

  redisReply *reply = (redisReply *) r;
  auto *cmd_obj = (Command<ReplyT> *) privdata;

  if (reply->type == REDIS_REPLY_ERROR) {
    std::cerr << "[ERROR] " << cmd_obj->cmd << ": " << reply->str << std::endl;
    cmd_obj->invoke_error(REDISX_ERROR_REPLY);
    return;
  }

  if(reply->type == REDIS_REPLY_NIL) {
    std::cerr << "[WARNING] " << cmd_obj->cmd << ": Nil reply." << std::endl;
    cmd_obj->invoke_error(REDISX_NIL_REPLY);
    return;
  }

  invoke_callback<ReplyT>(cmd_obj, reply);
}

template<class ReplyT>
Command<ReplyT>* Redis::command(
    const std::string& cmd,
    const std::function<void(const std::string&, const ReplyT&)>& callback,
    const std::function<void(const std::string&, int status)>& error_callback,
    double repeat,
    double after
) {
  std::lock_guard<std::mutex> lg(queue_guard);
  auto* cmd_obj = new Command<ReplyT>(c, cmd, callback, error_callback, repeat, after);
  get_command_map<ReplyT>()[(void*)cmd_obj] = cmd_obj;
  command_queue.push((void*)cmd_obj);
  return cmd_obj;
}

template<class ReplyT>
bool Redis::cancel(Command<ReplyT>* cmd_obj) {

  if(cmd_obj == NULL) {
    std::cerr << "[ERROR] Canceling null command." << std::endl;
    return false;
  }

  timer_callbacks.at(cmd_obj->timer) = NULL;

  std::lock_guard<std::mutex> lg(cmd_obj->timer_guard);
  if((cmd_obj->repeat != 0) || (cmd_obj->after != 0))
    ev_timer_stop(EV_DEFAULT_ cmd_obj->timer);

  delete cmd_obj->timer;
  cmd_obj->completed = true;

  return true;
}

template<class ReplyT>
ReplyT Redis::command_blocking(const std::string& cmd) {
  std::mutex m;
  std::condition_variable cv;
  ReplyT val;
  std::atomic_int status(REDISX_UNINIT);

  // There is a memory issue here, because after the callback returns
  // all memory is cleared.
  // TODO right now just don't use char* or redisReply* for blocking
  // Later, maybe specialize a function to copy the pointer types to
  // the heap

  command<ReplyT>(cmd,
    [&cv, &val, &status](const std::string& cmd_str, const ReplyT& reply) {
      std::cout << "cmd: " << cmd_str << std::endl;
      std::cout << "invoking success, reply: " << reply << std::endl;
      val = reply;
      std::cout << "invoking success, reply copied: " << val << std::endl;
      status = REDISX_OK;
      cv.notify_one();
    },
    [&cv, &status](const std::string& cmd_str, int error) {
      status = error;
      cv.notify_one();
    }
  );

  std::unique_lock<std::mutex> ul(m);
  cv.wait(ul, [&status] { return status != REDISX_UNINIT; });

  std::cout << "invoking success, after wait: " << val << std::endl;
  std::cout << "response: " << status << std::endl;
  if(status == REDISX_OK) return val;
  else if(status == REDISX_NIL_REPLY) return val;
  else throw std::runtime_error(
      "[ERROR] " + cmd + ": redisx error code " + std::to_string(status.load())
    );
}

} // End namespace redis
