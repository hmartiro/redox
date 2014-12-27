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
    double after = 0.0,
    bool free_memory = true
  );

  template<class ReplyT>
  bool cancel(Command<ReplyT>* cmd_obj);

  template<class ReplyT>
  Command<ReplyT>* command_blocking(const std::string& cmd);

  void command(const std::string& command);
  bool command_blocking(const std::string& command);

  long num_commands_processed();

//  void publish(std::string channel, std::string msg);
//  void subscribe(std::string channel, std::function<void(std::string channel, std::string msg)> callback);
//  void unsubscribe(std::string channel);

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
};

// ---------------------------

template<class ReplyT>
void invoke_callback(
    Command<ReplyT>* cmd_obj,
    redisReply* reply
);

template<class ReplyT>
void command_callback(redisAsyncContext *c, void *r, void *privdata) {

  auto *cmd_obj = (Command<ReplyT> *) privdata;
  cmd_obj->reply_obj = (redisReply *) r;

  if (cmd_obj->reply_obj->type == REDIS_REPLY_ERROR) {
    std::cerr << "[ERROR redisx.hpp:121] " << cmd_obj->cmd << ": " << cmd_obj->reply_obj->str << std::endl;
    cmd_obj->invoke_error(REDISX_ERROR_REPLY);

  } else if(cmd_obj->reply_obj->type == REDIS_REPLY_NIL) {
    std::cerr << "[WARNING] " << cmd_obj->cmd << ": Nil reply." << std::endl;
    cmd_obj->invoke_error(REDISX_NIL_REPLY);

  } else {
    invoke_callback<ReplyT>(cmd_obj, cmd_obj->reply_obj);
  }

  // Free the reply object unless told not to
  if(cmd_obj->free_memory) cmd_obj->free_reply_object();
}

template<class ReplyT>
Command<ReplyT>* Redis::command(
  const std::string& cmd,
  const std::function<void(const std::string&, const ReplyT&)>& callback,
  const std::function<void(const std::string&, int status)>& error_callback,
  double repeat,
  double after,
  bool free_memory
) {
  std::lock_guard<std::mutex> lg(queue_guard);
  auto* cmd_obj = new Command<ReplyT>(c, cmd, callback, error_callback, repeat, after, free_memory);
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

  cmd_obj->timer.data = NULL;

  std::lock_guard<std::mutex> lg(cmd_obj->timer_guard);
  if((cmd_obj->repeat != 0) || (cmd_obj->after != 0))
    ev_timer_stop(EV_DEFAULT_ &cmd_obj->timer);

  cmd_obj->completed = true;

  return true;
}

template<class ReplyT>
Command<ReplyT>* Redis::command_blocking(const std::string& cmd) {

  ReplyT val;
  std::atomic_int status(REDISX_UNINIT);

  std::condition_variable cv;
  std::mutex m;

  std::unique_lock<std::mutex> lk(m);

  Command<ReplyT>* cmd_obj = command<ReplyT>(cmd,
    [&val, &status, &m, &cv](const std::string& cmd_str, const ReplyT& reply) {
      std::unique_lock<std::mutex> ul(m);
      val = reply;
      status = REDISX_OK;
      ul.unlock();
      cv.notify_one();
    },
    [&status, &m, &cv](const std::string& cmd_str, int error) {
      std::unique_lock<std::mutex> ul(m);
      status = error;
      ul.unlock();
      cv.notify_one();
    },
    0, 0, false // No repeats, don't free memory
  );

  // Wait until a callback is invoked
  cv.wait(lk, [&status] { return status != REDISX_UNINIT; });

  cmd_obj->reply_val = val;
  cmd_obj->reply_status = status;

  return cmd_obj;
}

} // End namespace redis
