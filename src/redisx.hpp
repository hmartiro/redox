/**
* Redis C++11 wrapper.
*/

#pragma once

#include <functional>
#include <string>
#include <iostream>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>

namespace redisx {

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

  struct event* command_loop(const char* command, long interval_s, long interval_us);

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

  auto *cmd_obj = new CommandAsync<ReplyT>(cmd, callback);

  int status = redisAsyncCommand(c, command_callback<ReplyT>, (void*)cmd_obj, cmd.c_str());
  if (status != REDIS_OK) {
    std::cerr << "[ERROR] Async command \"" << cmd << "\": " << c->errstr << std::endl;
    delete cmd_obj;
    return;
  }
}


} // End namespace redis
