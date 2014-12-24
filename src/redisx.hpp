/**
* Redis C++11 wrapper.
*/

#pragma once

#include <functional>
#include <string>
#include <iostream>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <mutex>
#include <queue>
#include <set>
#include <unordered_map>

namespace redisx {

class CommandAsyncGeneric {
public:
  redisCallbackFn* fn;
  void* privdata;
  const char* cmd;
  CommandAsyncGeneric(redisCallbackFn* fn, void* privdata, const char* cmd)
      : fn(fn), privdata(privdata), cmd(cmd) {}
};

template<class ReplyT>
class CommandAsync {
public:
  CommandAsync(const std::string& cmd, const std::function<void(const std::string&, ReplyT)>& callback)
      : cmd(cmd), callback(callback) {}
  const std::string cmd;
  const std::function<void(const std::string&, ReplyT)> callback;
  void invoke(ReplyT reply) const {if(callback != NULL) callback(cmd, reply); }
};

class Redis {

public:

  Redis(const std::string& host, const int port);
  ~Redis();

  void start();

  template<class ReplyT>
  void command(
      const std::string& cmd,
      const std::function<void(const std::string&, ReplyT)>& callback
  );

  void command(const char* command);

//  struct event* command_loop(const char* command, long interval_s, long interval_us);

  void get(const char* key, std::function<void(const std::string&, const char*)> callback);

  void set(const char* key, const char* value);
  void set(const char* key, const char* value, std::function<void(const std::string&, const char*)> callback);

  void del(const char* key);
  void del(const char* key, std::function<void(const std::string&, long long int)> callback);

//  void publish(std::string channel, std::string msg);
//  void subscribe(std::string channel, std::function<void(std::string channel, std::string msg)> callback);
//  void unsubscribe(std::string channel);

private:

  // Redis server
  std::string host;
  int port;

  // Number of IOs performed
  long io_ops;

  struct event_base *base;
  redisAsyncContext *c;

  std::mutex evlock;

  std::thread event_loop_thread;

  template<class ReplyT>
  std::unordered_map<void*, CommandAsync<ReplyT>*>& get_command_map();

  std::unordered_map<void*, CommandAsync<const redisReply*>*> commands_redis_reply;
  std::unordered_map<void*, CommandAsync<const std::string&>*> commands_string_r;
  std::unordered_map<void*, CommandAsync<const char*>*> commands_char_p;
  std::unordered_map<void*, CommandAsync<int>*> commands_int;
  std::unordered_map<void*, CommandAsync<long long int>*> commands_long_long_int;

  std::queue<void*> command_queue;
  void process_queued_commands();

  template<class ReplyT>
  bool process_queued_command(void* cmd_ptr);
};

// ---------------------------

template<class ReplyT>
void invoke_callback(
    const CommandAsync<ReplyT>* cmd_obj,
    redisReply* reply
);

template<class ReplyT>
void command_callback(redisAsyncContext *c, void *r, void *privdata) {

  redisReply *reply = (redisReply *) r;
  auto *cmd_obj = (CommandAsync<ReplyT> *) privdata;

  if (reply->type == REDIS_REPLY_ERROR) {
    std::cerr << "[ERROR] " << cmd_obj->cmd << ": " << reply->str << std::endl;
    delete cmd_obj;
    return;
  }

  if(reply->type == REDIS_REPLY_NIL) {
    std::cerr << "[ERROR] " << cmd_obj->cmd << ": Nil reply." << std::endl;
    delete cmd_obj;
    return; // cmd_obj->invoke(NULL);
  }

  invoke_callback<ReplyT>(cmd_obj, reply);
  delete cmd_obj;
}

template<class ReplyT>
void Redis::command(const std::string& cmd, const std::function<void(const std::string&, ReplyT)>& callback) {

  std::lock_guard<std::mutex> lg(evlock);
  auto* cmd_obj = new CommandAsync<ReplyT>(cmd, callback);
  get_command_map<ReplyT>()[(void*)cmd_obj] = cmd_obj;
  command_queue.push((void*)cmd_obj);
}

} // End namespace redis
