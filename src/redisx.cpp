/**
* Redis C++11 wrapper.
*/

#include <signal.h>
#include <iostream>
#include <thread>
#include <hiredis/adapters/libev.h>
#include <ev.h>
#include <event2/thread.h>
#include <vector>
#include <string.h>
#include "redisx.hpp"

using namespace std;

namespace redisx {

mutex connected_lock;

void connected(const redisAsyncContext *c, int status) {
  if (status != REDIS_OK) {
    printf("Error: %s\n", c->errstr);
    return;
  }
  printf("Connected...\n");
  connected_lock.unlock();
}

void disconnected(const redisAsyncContext *c, int status) {
  if (status != REDIS_OK) {
    printf("Error: %s\n", c->errstr);
    return;
  }
  printf("Disconnected...\n");
  connected_lock.lock();
}

Redis::Redis(const string& host, const int port) : host(host), port(port), io_ops(0) {

//  evthread_use_pthreads();
//  evthread_enable_lock_debuging();
//  event_enable_debug_mode();

  lock_guard<mutex> lg(evlock);
  connected_lock.lock();

  signal(SIGPIPE, SIG_IGN);
//  base = event_base_new();

  c = redisAsyncConnect(host.c_str(), port);
  if (c->err) {
    printf("Error: %s\n", c->errstr);
    return;
  }

  redisLibevAttach(EV_DEFAULT_ c);
//  redisLibeventAttach(c, base);
  redisAsyncSetConnectCallback(c, connected);
  redisAsyncSetDisconnectCallback(c, disconnected);
}

Redis::~Redis() {
  redisAsyncDisconnect(c);
}

void Redis::start() {

  event_loop_thread = thread([this] {
    ev_run(EV_DEFAULT_ EVRUN_NOWAIT);
    connected_lock.lock();

    while (true) {
      process_queued_commands();
      ev_run(EV_DEFAULT_ EVRUN_NOWAIT);
    }
  });
  event_loop_thread.detach();
}

template<class ReplyT>
bool Redis::process_queued_command(void* cmd_ptr) {

  auto& command_map = get_command_map<ReplyT>();

  auto it = command_map.find(cmd_ptr);
  if(it == command_map.end()) return false;
  CommandAsync<ReplyT>* cmd_obj = it->second;
  command_map.erase(cmd_ptr);

  if (redisAsyncCommand(c, command_callback<ReplyT>, cmd_ptr, cmd_obj->cmd.c_str()) != REDIS_OK) {
    cerr << "[ERROR] Async command \"" << cmd_obj->cmd << "\": " << c->errstr << endl;
    delete cmd_obj;
  }

  return true;
}

void Redis::process_queued_commands() {
  lock_guard<mutex> lg(evlock);

  while(!command_queue.empty()) {

    void* cmd_ptr = command_queue.front();
    if(process_queued_command<const redisReply*>(cmd_ptr)) {}
    else if(process_queued_command<const string&>(cmd_ptr)) {}
    else if(process_queued_command<const char*>(cmd_ptr)) {}
    else if(process_queued_command<int>(cmd_ptr)) {}
    else if(process_queued_command<long long int>(cmd_ptr)) {}
    else throw runtime_error("[FATAL] Command pointer not found in any queue!");

    command_queue.pop();
  }
}

// ----------------------------

// TODO update
void Redis::command(const char* cmd) {

  evlock.lock();
  int status = redisAsyncCommand(c, NULL, NULL, cmd);
  evlock.unlock();

  if (status != REDIS_OK) {
    cerr << "[ERROR] Async command \"" << cmd << "\": " << c->errstr << endl;
    return;
  }
}

// ----------------------------

template<> unordered_map<void*, CommandAsync<const redisReply*>*>& Redis::get_command_map() { return commands_redis_reply; }
template<>
void invoke_callback(const CommandAsync<const redisReply*>* cmd_obj, redisReply* reply) {
  cmd_obj->invoke(reply);
}

template<> unordered_map<void*, CommandAsync<const string&>*>& Redis::get_command_map() { return commands_string_r; }
template<>
void invoke_callback(const CommandAsync<const string&>* cmd_obj, redisReply* reply) {
  if(reply->type != REDIS_REPLY_STRING && reply->type != REDIS_REPLY_STATUS) {
    cerr << "[ERROR] " << cmd_obj->cmd << ": Received non-string reply." << endl;
    return;
  }

  cmd_obj->invoke(reply->str);
}

template<> unordered_map<void*, CommandAsync<const char*>*>& Redis::get_command_map() { return commands_char_p; }
template<>
void invoke_callback(const CommandAsync<const char*>* cmd_obj, redisReply* reply) {
  if(reply->type != REDIS_REPLY_STRING && reply->type != REDIS_REPLY_STATUS) {
    cerr << "[ERROR] " << cmd_obj->cmd << ": Received non-string reply." << endl;
    return;
  }
  cmd_obj->invoke(reply->str);
}

template<> unordered_map<void*, CommandAsync<int>*>& Redis::get_command_map() { return commands_int; }
template<>
void invoke_callback(const CommandAsync<int>* cmd_obj, redisReply* reply) {
  if(reply->type != REDIS_REPLY_INTEGER) {
    cerr << "[ERROR] " << cmd_obj->cmd << ": Received non-integer reply." << endl;
    return;
  }
  cmd_obj->invoke((int)reply->integer);
}

template<> unordered_map<void*, CommandAsync<long long int>*>& Redis::get_command_map() { return commands_long_long_int; }
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

void Redis::set(const char* key, const char* value, function<void(const string&, const char*)> callback) {
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

void Redis::del(const char* key, function<void(const string&, long long int)> callback) {
  string cmd = string("DEL ") + key;
  command<long long int>(cmd.c_str(), callback);
}

} // End namespace redis
