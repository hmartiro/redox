/**
* Redis C++11 wrapper.
*/

#include <signal.h>
#include <iostream>
#include <thread>
#include <hiredis/adapters/libevent.h>
#include <vector>
#include <string.h>
#include "redisx.hpp"

using namespace std;

namespace redisx {

void connected(const redisAsyncContext *c, int status) {
  if (status != REDIS_OK) {
    printf("Error: %s\n", c->errstr);
    return;
  }
  printf("Connected...\n");
}

void disconnected(const redisAsyncContext *c, int status) {
  if (status != REDIS_OK) {
    printf("Error: %s\n", c->errstr);
    return;
  }
  printf("Disconnected...\n");
}

Redis::Redis(const string& host, const int port) : host(host), port(port), io_ops(0) {

  signal(SIGPIPE, SIG_IGN);
  base = event_base_new();

  c = redisAsyncConnect(host.c_str(), port);
  if (c->err) {
    printf("Error: %s\n", c->errstr);
    return;
  }

  redisLibeventAttach(c, base);
  redisAsyncSetConnectCallback(c, connected);
  redisAsyncSetDisconnectCallback(c, disconnected);
}

Redis::~Redis() {
  redisAsyncDisconnect(c);
}

void Redis::start() {
  event_base_dispatch(base);
}

void Redis::command(const char* cmd) {
  int status = redisAsyncCommand(c, NULL, NULL, cmd);
  if (status != REDIS_OK) {
    cerr << "[ERROR] Async command \"" << cmd << "\": " << c->errstr << endl;
    return;
  }
}

// ----------------------------

template<>
void invoke_callback(const CommandAsync<const redisReply*>* cmd_obj, redisReply* reply) {
  cmd_obj->invoke(reply);
}

template<>
void invoke_callback(const CommandAsync<const string&>* cmd_obj, redisReply* reply) {
  if(reply->type != REDIS_REPLY_STRING && reply->type != REDIS_REPLY_STATUS) {
    cerr << "[ERROR] " << cmd_obj->cmd << ": Received non-string reply." << endl;
    return;
  }

  cmd_obj->invoke(reply->str);
}

template<>
void invoke_callback(const CommandAsync<const char*>* cmd_obj, redisReply* reply) {
  if(reply->type != REDIS_REPLY_STRING && reply->type != REDIS_REPLY_STATUS) {
    cerr << "[ERROR] " << cmd_obj->cmd << ": Received non-string reply." << endl;
    return;
  }
  cmd_obj->invoke(reply->str);
}

template<>
void invoke_callback(const CommandAsync<int>* cmd_obj, redisReply* reply) {
  if(reply->type != REDIS_REPLY_INTEGER) {
    cerr << "[ERROR] " << cmd_obj->cmd << ": Received non-integer reply." << endl;
    return;
  }
  cmd_obj->invoke((int)reply->integer);
}

template<>
void invoke_callback(const CommandAsync<long long int>* cmd_obj, redisReply* reply) {
  if(reply->type != REDIS_REPLY_INTEGER) {
    cerr << "[ERROR] " << cmd_obj->cmd << ": Received non-integer reply." << endl;
    return;
  }
  cmd_obj->invoke(reply->integer);
}

// ----------------------------

void Redis::get(const char* key, function<void(const string&, const char*)> callback) {
  string cmd = string("GET ") + key;
  command<const char*>(cmd.c_str(), callback);
}

void Redis::set(const char* key, const char* value) {
  string cmd = string("SET ") + key + " " + value;
  command<const char*>(cmd.c_str(), [](const string& command, const char* reply) {
    if(strcmp(reply, "OK"))
      cerr << "[ERROR] " << command << ": SET failed with reply " << reply << endl;
  });
}

void Redis::set(const char* key, const char* value, std::function<void(const string&, const char*)> callback) {
  string cmd = string("SET ") + key + " " + value;
  command<const char*>(cmd.c_str(), callback);
}

void Redis::del(const char* key) {
  string cmd = string("DEL ") + key;
  command<long long int>(cmd.c_str(), [](const string& command, long long int num_deleted) {
    if(num_deleted != 1)
      cerr << "[ERROR] " << command << ": Deleted " << num_deleted << " keys." << endl;
  });
}

void Redis::del(const char* key, std::function<void(const string&, long long int)> callback) {
  string cmd = string("DEL ") + key;
  command<long long int>(cmd.c_str(), callback);
}

} // End namespace redis
