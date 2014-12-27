/**
* Redis C++11 wrapper.
*/

#include <signal.h>
#include <string.h>
//#include <event2/thread.h>
#include "redisx.hpp"

using namespace std;

namespace redisx {

// Global mutex to manage waiting for connected state
// TODO get rid of this as the only global variable?
mutex connected_lock;

/**
* Dummy function given to hiredis to use for freeing reply
* objects, so the memory can be managed here instead.
*/
void dummy_free_reply(void *reply) {}

void connected(const redisAsyncContext *c, int status) {
  if (status != REDIS_OK) {
    cerr << "[ERROR] Connecting to Redis: " << c->errstr << endl;
    return;
  }

  c->c.reader->fn->freeObject = dummy_free_reply;
  cout << "Connected to Redis." << endl;
  connected_lock.unlock();
}

void disconnected(const redisAsyncContext *c, int status) {
  if (status != REDIS_OK) {
    cerr << "[ERROR] Disconnecting from Redis: " << c->errstr << endl;
    return;
  }
  c->c.reader->fn->freeObject = freeReplyObject;
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
  exit_waiter.notify_one();
}

void Redis::run() {

  event_loop_thread = thread([this] { run_blocking(); });
  event_loop_thread.detach();
}

void Redis::stop() {
  to_exit = true;
}

void Redis::block() {
  unique_lock<mutex> ul(exit_waiter_lock);
  exit_waiter.wait(ul, [this]() { return to_exit.load(); });
}

/**
* Submit an asynchronous command to the Redis server. Return
* true if succeeded, false otherwise.
*/
template<class ReplyT>
bool submit_to_server(Command<ReplyT>* cmd_obj) {
  cmd_obj->pending++;
  if (redisAsyncCommand(cmd_obj->c, command_callback<ReplyT>, (void*)cmd_obj, cmd_obj->cmd.c_str()) != REDIS_OK) {
    cerr << "[ERROR] Could not send \"" << cmd_obj->cmd << "\": " << cmd_obj->c->errstr << endl;
    cmd_obj->invoke_error(REDISX_SEND_ERROR);
    return false;
  }
  return true;
}

template<class ReplyT>
void submit_command_callback(struct ev_loop* loop, ev_timer* timer, int revents) {

  // Check if canceled
  if(timer->data == NULL) {
    cerr << "[WARNING] Skipping event, has been canceled." << endl;
    return;
  }

  auto cmd_obj = (Command<ReplyT>*)timer->data;
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

    cmd_obj->timer.data = (void*)cmd_obj;

    ev_timer_init(&cmd_obj->timer, submit_command_callback<ReplyT>, cmd_obj->after, cmd_obj->repeat);
    ev_timer_start(EV_DEFAULT_ &cmd_obj->timer);

    cmd_obj->timer_guard.unlock();
  }

  return true;
}

void Redis::process_queued_commands() {

  lock_guard<mutex> lg(queue_guard);

  while(!command_queue.empty()) {

    void* cmd_ptr = command_queue.front();
    if(process_queued_command<redisReply*>(cmd_ptr)) {}
    else if(process_queued_command<string>(cmd_ptr)) {}
    else if(process_queued_command<char*>(cmd_ptr)) {}
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

template<> unordered_map<void*, Command<redisReply*>*>& Redis::get_command_map() { return commands_redis_reply; }
template<>
void invoke_callback(Command<redisReply*>* cmd_obj, redisReply* reply) {
  cmd_obj->invoke(reply);
}

template<> unordered_map<void*, Command<string>*>& Redis::get_command_map() { return commands_string_r; }
template<>
void invoke_callback(Command<string>* cmd_obj, redisReply* reply) {
  if(reply->type != REDIS_REPLY_STRING && reply->type != REDIS_REPLY_STATUS) {
    cerr << "[ERROR] " << cmd_obj->cmd << ": Received non-string reply." << endl;
    cmd_obj->invoke_error(REDISX_WRONG_TYPE);
    return;
  }

  string s = reply->str;
  cmd_obj->invoke(s);
}

template<> unordered_map<void*, Command<char*>*>& Redis::get_command_map() { return commands_char_p; }
template<>
void invoke_callback(Command<char*>* cmd_obj, redisReply* reply) {
  if(reply->type != REDIS_REPLY_STRING && reply->type != REDIS_REPLY_STATUS) {
    cerr << "[ERROR] " << cmd_obj->cmd << ": Received non-string reply." << endl;
    cmd_obj->invoke_error(REDISX_WRONG_TYPE);
    return;
  }
  cmd_obj->invoke(reply->str);
}

template<> unordered_map<void*, Command<int>*>& Redis::get_command_map() { return commands_int; }
template<>
void invoke_callback(Command<int>* cmd_obj, redisReply* reply) {
  if(reply->type != REDIS_REPLY_INTEGER) {
    cerr << "[ERROR] " << cmd_obj->cmd << ": Received non-integer reply." << endl;
    cmd_obj->invoke_error(REDISX_WRONG_TYPE);
    return;
  }
  cmd_obj->invoke((int)reply->integer);
}

template<> unordered_map<void*, Command<long long int>*>& Redis::get_command_map() { return commands_long_long_int; }
template<>
void invoke_callback(Command<long long int>* cmd_obj, redisReply* reply) {
  if(reply->type != REDIS_REPLY_INTEGER) {
    cerr << "[ERROR] " << cmd_obj->cmd << ": Received non-integer reply." << endl;
    cmd_obj->invoke_error(REDISX_WRONG_TYPE);
    return;
  }
  cmd_obj->invoke(reply->integer);
}

// ----------------------------
// Helpers
// ----------------------------

void Redis::command(const string& cmd) {
  command<redisReply*>(cmd, NULL);
}

bool Redis::command_blocking(const string& cmd) {
  Command<redisReply*>* c = command_blocking<redisReply*>(cmd);
  bool succeeded = (c->status() == REDISX_OK);
  c->free();
  return succeeded;
}

} // End namespace redis
