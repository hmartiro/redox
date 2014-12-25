/**
* Redis C++11 wrapper.
*/

#include <signal.h>
//#include <event2/thread.h>
#include "redisx.hpp"

using namespace std;

namespace redisx {

// Global mutex to manage waiting for connected state
mutex connected_lock;

// Map of ev_timer events to pointers to Command objects
// Used to get the object back from the timer watcher callback
unordered_map<ev_timer*, void*> timer_callbacks;

void connected(const redisAsyncContext *c, int status) {
  if (status != REDIS_OK) {
    cerr << "[ERROR] Connecting to Redis: " << c->errstr << endl;
    return;
  }
  cout << "Connected to Redis." << endl;
  connected_lock.unlock();
}

void disconnected(const redisAsyncContext *c, int status) {
  if (status != REDIS_OK) {
    cerr << "[ERROR] Disconnecting from Redis: " << c->errstr << endl;
    return;
  }
  cout << "Disconnected from Redis." << endl;
  connected_lock.lock();
}

Redis::Redis(const string& host, const int port)
    : host(host), port(port), cmd_count(0), to_exit(false) {

  lock_guard<mutex> lg(queue_guard);
  connected_lock.lock();

  signal(SIGPIPE, SIG_IGN);

  c = redisAsyncConnect(host.c_str(), port);
  if (c->err) {
    printf("Error: %s\n", c->errstr);
    return;
  }

  redisLibevAttach(EV_DEFAULT_ c);
  redisAsyncSetConnectCallback(c, connected);
  redisAsyncSetDisconnectCallback(c, disconnected);
}

Redis::~Redis() {
  redisAsyncDisconnect(c);
  stop();
}

void Redis::run_blocking() {

  // Events to connect to Redis
  ev_run(EV_DEFAULT_ EVRUN_NOWAIT);
  lock_guard<mutex> lg(connected_lock);

  // Continuously create events and handle them
  while (!to_exit) {
    process_queued_commands();
    ev_run(EV_DEFAULT_ EVRUN_NOWAIT);
  }

  // Handle exit events
  ev_run(EV_DEFAULT_ EVRUN_NOWAIT);

  // Let go for block_until_stopped method
  unique_lock<mutex> ul(exit_waiter_lock);
  exit_waiter.notify_one();
}

void Redis::run() {

  event_loop_thread = thread([this] { run_blocking(); });
  event_loop_thread.detach();
}

void Redis::stop() {
  to_exit = true;
}

void Redis::block_until_stopped() {
  unique_lock<mutex> ul(exit_waiter_lock);
  exit_waiter.wait(ul, [this]() { return to_exit.load(); });
}

/**
* Submit an asynchronous command to the Redis server. Return
* true if succeeded, false otherwise.
*/
template<class ReplyT>
bool submit_to_server(Command<ReplyT>* cmd_obj) {
  if (redisAsyncCommand(cmd_obj->c, command_callback<ReplyT>, (void*)cmd_obj, cmd_obj->cmd.c_str()) != REDIS_OK) {
    cerr << "[ERROR] Async command \"" << cmd_obj->cmd << "\": " << cmd_obj->c->errstr << endl;
    cmd_obj->free_if_done();
    return false;
  }
  return true;
}

template<class ReplyT>
void submit_command_callback(struct ev_loop* loop, ev_timer* timer, int revents) {
  auto cmd_obj = (Command<ReplyT>*)timer_callbacks.at(timer);
  submit_to_server<ReplyT>(cmd_obj);
}

template<class ReplyT>
bool Redis::process_queued_command(void* cmd_ptr) {

  auto& command_map = get_command_map<ReplyT>();

  auto it = command_map.find(cmd_ptr);
  if(it == command_map.end()) return false;
  Command<ReplyT>* cmd_obj = it->second;
  command_map.erase(cmd_ptr);

  if((cmd_obj->repeat == 0) && (cmd_obj->after == 0)) {
    submit_to_server<ReplyT>(cmd_obj);
  } else {
    cmd_obj->timer = new ev_timer();
    timer_callbacks[cmd_obj->timer] = (void*)cmd_obj;
    ev_timer_init(cmd_obj->timer, submit_command_callback<ReplyT>, cmd_obj->after, cmd_obj->repeat);
    ev_timer_start(EV_DEFAULT_ cmd_obj->timer);

    cmd_obj->timer_guard.unlock();
  }

  return true;
}


void Redis::process_queued_commands() {

  lock_guard<mutex> lg(queue_guard);

  while(!command_queue.empty()) {

    void* cmd_ptr = command_queue.front();
    if(process_queued_command<const redisReply*>(cmd_ptr)) {}
    else if(process_queued_command<const string&>(cmd_ptr)) {}
    else if(process_queued_command<const char*>(cmd_ptr)) {}
    else if(process_queued_command<int>(cmd_ptr)) {}
    else if(process_queued_command<long long int>(cmd_ptr)) {}
    else throw runtime_error("[FATAL] Command pointer not found in any queue!");

    command_queue.pop();
    cmd_count++;
  }
}

long Redis::num_commands_processed() {
  lock_guard<mutex> lg(queue_guard);
  return cmd_count;
}

// ----------------------------

template<> unordered_map<void*, Command<const redisReply*>*>& Redis::get_command_map() { return commands_redis_reply; }
template<>
void invoke_callback(Command<const redisReply*>* cmd_obj, redisReply* reply) {
  cmd_obj->invoke(reply);
}

template<> unordered_map<void*, Command<const string&>*>& Redis::get_command_map() { return commands_string_r; }
template<>
void invoke_callback(Command<const string&>* cmd_obj, redisReply* reply) {
  if(reply->type != REDIS_REPLY_STRING && reply->type != REDIS_REPLY_STATUS) {
    cerr << "[ERROR] " << cmd_obj->cmd << ": Received non-string reply." << endl;
    return;
  }

  cmd_obj->invoke(reply->str);
}

template<> unordered_map<void*, Command<const char*>*>& Redis::get_command_map() { return commands_char_p; }
template<>
void invoke_callback(Command<const char*>* cmd_obj, redisReply* reply) {
  if(reply->type != REDIS_REPLY_STRING && reply->type != REDIS_REPLY_STATUS) {
    cerr << "[ERROR] " << cmd_obj->cmd << ": Received non-string reply." << endl;
    return;
  }
  cmd_obj->invoke(reply->str);
}

template<> unordered_map<void*, Command<int>*>& Redis::get_command_map() { return commands_int; }
template<>
void invoke_callback(Command<int>* cmd_obj, redisReply* reply) {
  if(reply->type != REDIS_REPLY_INTEGER) {
    cerr << "[ERROR] " << cmd_obj->cmd << ": Received non-integer reply." << endl;
    return;
  }
  cmd_obj->invoke((int)reply->integer);
}

template<> unordered_map<void*, Command<long long int>*>& Redis::get_command_map() { return commands_long_long_int; }
template<>
void invoke_callback(Command<long long int>* cmd_obj, redisReply* reply) {
  if(reply->type != REDIS_REPLY_INTEGER) {
    cerr << "[ERROR] " << cmd_obj->cmd << ": Received non-integer reply." << endl;
    return;
  }
  cmd_obj->invoke(reply->integer);
}

// ----------------------------
// Helpers
// ----------------------------

void Redis::command(const char* cmd) {
  command<const redisReply*>(cmd, NULL);
}

//void Redis::get(const char* key, function<void(const string&, const char*)> callback) {
//  string cmd = string("GET ") + key;
//  command<const char*>(cmd.c_str(), callback);
//}
//
//void Redis::set(const char* key, const char* value) {
//  string cmd = string("SET ") + key + " " + value;
//  command<const char*>(cmd.c_str(), [](const string& command, const char* reply) {
//    if(strcmp(reply, "OK"))
//      cerr << "[ERROR] " << command << ": SET failed with reply " << reply << endl;
//  });
//}
//
//void Redis::set(const char* key, const char* value, function<void(const string&, const char*)> callback) {
//  string cmd = string("SET ") + key + " " + value;
//  command<const char*>(cmd.c_str(), callback);
//}
//
//void Redis::del(const char* key) {
//  string cmd = string("DEL ") + key;
//  command<long long int>(cmd.c_str(), [](const string& command, long long int num_deleted) {
//    if(num_deleted != 1)
//      cerr << "[ERROR] " << command << ": Deleted " << num_deleted << " keys." << endl;
//  });
//}
//
//void Redis::del(const char* key, function<void(const string&, long long int)> callback) {
//  string cmd = string("DEL ") + key;
//  command<long long int>(cmd.c_str(), callback);
//}

} // End namespace redis
