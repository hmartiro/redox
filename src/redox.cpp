/**
* Redis C++11 wrapper.
*/

#include <signal.h>
#include <string.h>
#include "redox.hpp"

using namespace std;

namespace redox {

// Global mutex to manage waiting for connected state
// TODO get rid of this as the only global variable?
mutex connected_lock;

void Redox::connected(const redisAsyncContext *c, int status) {
  if (status != REDIS_OK) {
    cerr << "[ERROR] Connecting to Redis: " << c->errstr << endl;
    return;
  }

  // Disable hiredis automatically freeing reply objects
  c->c.reader->fn->freeObject = [](void* reply) {};

  cout << "Connected to Redis." << endl;
  connected_lock.unlock();
}

void Redox::disconnected(const redisAsyncContext *c, int status) {
  if (status != REDIS_OK) {
    cerr << "[ERROR] Disconnecting from Redis: " << c->errstr << endl;
    return;
  }

  // Re-enable hiredis automatically freeing reply objects
  c->c.reader->fn->freeObject = freeReplyObject;

  cout << "Disconnected from Redis." << endl;
  connected_lock.lock();
}

Redox::Redox(const string& host, const int port)
    : host(host), port(port), cmd_count(0), to_exit(false) {

  lock_guard<mutex> lg(queue_guard);
  connected_lock.lock();

  signal(SIGPIPE, SIG_IGN);

  ctx = redisAsyncConnect(host.c_str(), port);
  if (ctx->err) {
    printf("Error: %s\n", ctx->errstr);
    return;
  }

  redisLibevAttach(EV_DEFAULT_ ctx);
  redisAsyncSetConnectCallback(ctx, Redox::connected);
  redisAsyncSetDisconnectCallback(ctx, Redox::disconnected);
}

Redox::~Redox() {
  redisAsyncDisconnect(ctx);
  stop();
}

void Redox::run_blocking() {

  // Events to connect to Redox
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

void Redox::run() {

  event_loop_thread = thread([this] { run_blocking(); });
  event_loop_thread.detach();
}

void Redox::stop() {
  to_exit = true;
}

void Redox::block() {
  unique_lock<mutex> ul(exit_waiter_lock);
  exit_waiter.wait(ul, [this]() { return to_exit.load(); });
}

/**
* Submit an asynchronous command to the Redox server. Return
* true if succeeded, false otherwise.
*/
template<class ReplyT>
bool submit_to_server(Command<ReplyT>* c) {
  c->pending++;
  if (redisAsyncCommand(c->rdx->ctx, c->command_callback, (void*)c, c->cmd.c_str()) != REDIS_OK) {
    cerr << "[ERROR] Could not send \"" << c->cmd << "\": " << c->rdx->ctx->errstr << endl;
    c->invoke_error(REDOX_SEND_ERROR);
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

  auto c = (Command<ReplyT>*)timer->data;
  submit_to_server<ReplyT>(c);
}

template<class ReplyT>
bool Redox::process_queued_command(void* c_ptr) {

  auto& command_map = get_command_map<ReplyT>();

  auto it = command_map.find(c_ptr);
  if(it == command_map.end()) return false;
  Command<ReplyT>* c = it->second;
  command_map.erase(c_ptr);

  if((c->repeat == 0) && (c->after == 0)) {
    submit_to_server<ReplyT>(c);
  } else {

    c->timer.data = (void*)c;

    ev_timer_init(&c->timer, submit_command_callback<ReplyT>, c->after, c->repeat);
    ev_timer_start(EV_DEFAULT_ &c->timer);

    c->timer_guard.unlock();
  }

  return true;
}

void Redox::process_queued_commands() {

  lock_guard<mutex> lg(queue_guard);

  while(!command_queue.empty()) {

    void* c_ptr = command_queue.front();
    if(process_queued_command<redisReply*>(c_ptr)) {}
    else if(process_queued_command<string>(c_ptr)) {}
    else if(process_queued_command<char*>(c_ptr)) {}
    else if(process_queued_command<int>(c_ptr)) {}
    else if(process_queued_command<long long int>(c_ptr)) {}
    else if(process_queued_command<nullptr_t>(c_ptr)) {}
    else throw runtime_error("[FATAL] Command pointer not found in any queue!");

    command_queue.pop();
  }
}

// ----------------------------

template<> unordered_map<void*, Command<redisReply*>*>&
Redox::get_command_map() { return commands_redis_reply; }

template<> unordered_map<void*, Command<string>*>&
Redox::get_command_map() { return commands_string_r; }

template<> unordered_map<void*, Command<char*>*>&
Redox::get_command_map() { return commands_char_p; }

template<> unordered_map<void*, Command<int>*>&
Redox::get_command_map() { return commands_int; }

template<> unordered_map<void*, Command<long long int>*>&
Redox::get_command_map() { return commands_long_long_int; }

template<> unordered_map<void*, Command<nullptr_t>*>&
Redox::get_command_map() { return commands_null; }

// ----------------------------
// Helpers
// ----------------------------

void Redox::command(const string& cmd) {
  command<redisReply*>(cmd, NULL);
}

bool Redox::command_blocking(const string& cmd) {
  Command<redisReply*>* c = command_blocking<redisReply*>(cmd);
  bool succeeded = (c->status() == REDOX_OK);
  c->free();
  return succeeded;
}

} // End namespace redis
