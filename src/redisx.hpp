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

namespace redisx {

template<class ReplyT>
class CommandAsync {
public:
  CommandAsync(
      redisAsyncContext* c,
      const std::string& cmd,
      const std::function<void(const std::string&, ReplyT)>& callback,
      double repeat, double after
  ) : c(c), cmd(cmd), callback(callback), repeat(repeat), after(after), done(false) {}

  redisAsyncContext* c;
  const std::string cmd;
  const std::function<void(const std::string&, ReplyT)> callback;
  double repeat;
  double after;
  bool done;
  ev_timer* timer;

  void invoke(ReplyT reply) {
    if(callback != NULL) callback(cmd, reply);
    if((repeat == 0)) done = true;
  }
  void free_if_done() {
    if(done) {
      std::cout << "Deleting CommandAsync: " << cmd << std::endl;
      delete this;
    };
  }
};

class Redis {

public:

  Redis(const std::string& host, const int port);
  ~Redis();

  void run();
  void run_blocking();
  void stop();
  void block_until_stopped();

  template<class ReplyT>
  void command(
      const std::string& cmd,
      const std::function<void(const std::string&, ReplyT)>& callback = NULL,
      double repeat = 0.0,
      double after = 0.0
  );

  void command(const char* command);

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

  std::unordered_map<void*, CommandAsync<const redisReply*>*> commands_redis_reply;
  std::unordered_map<void*, CommandAsync<const std::string&>*> commands_string_r;
  std::unordered_map<void*, CommandAsync<const char*>*> commands_char_p;
  std::unordered_map<void*, CommandAsync<int>*> commands_int;
  std::unordered_map<void*, CommandAsync<long long int>*> commands_long_long_int;

  template<class ReplyT>
  std::unordered_map<void*, CommandAsync<ReplyT>*>& get_command_map();

  std::queue<void*> command_queue;
  std::mutex queue_guard;
  void process_queued_commands();

  template<class ReplyT>
  bool process_queued_command(void* cmd_ptr);
};

// ---------------------------

template<class ReplyT>
void invoke_callback(
    CommandAsync<ReplyT>* cmd_obj,
    redisReply* reply
);

template<class ReplyT>
void command_callback(redisAsyncContext *c, void *r, void *privdata) {

  redisReply *reply = (redisReply *) r;
  auto *cmd_obj = (CommandAsync<ReplyT> *) privdata;

  if (reply->type == REDIS_REPLY_ERROR) {
    std::cerr << "[ERROR] " << cmd_obj->cmd << ": " << reply->str << std::endl;
    cmd_obj->free_if_done();
    return;
  }

  if(reply->type == REDIS_REPLY_NIL) {
    std::cerr << "[WARNING] " << cmd_obj->cmd << ": Nil reply." << std::endl;
    cmd_obj->free_if_done();
    return; // cmd_obj->invoke(NULL);
  }

  invoke_callback<ReplyT>(cmd_obj, reply);
  cmd_obj->free_if_done();
}

template<class ReplyT>
void Redis::command(
    const std::string& cmd,
    const std::function<void(const std::string&, ReplyT)>& callback,
    double repeat,
    double after
) {

  std::lock_guard<std::mutex> lg(queue_guard);
  auto* cmd_obj = new CommandAsync<ReplyT>(c, cmd, callback, repeat, after);
  get_command_map<ReplyT>()[(void*)cmd_obj] = cmd_obj;
  command_queue.push((void*)cmd_obj);
}

} // End namespace redis
