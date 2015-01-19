/**
* Redis C++11 wrapper.
*/

#include <signal.h>
#include "redox.hpp"

using namespace std;

namespace redox {

void Redox::connected_callback(const redisAsyncContext *ctx, int status) {

  Redox* rdx = (Redox*) ctx->data;

  if (status != REDIS_OK) {
    cerr << "[ERROR] Connecting to Redis: " << ctx->errstr << endl;
    rdx->connect_state = REDOX_CONNECT_ERROR;

  } else {
    // Disable hiredis automatically freeing reply objects
    ctx->c.reader->fn->freeObject = [](void *reply) {};
    rdx->connect_state = REDOX_CONNECTED;
    cout << "[INFO] Connected to Redis." << endl;
  }

  rdx->connect_waiter.notify_all();
  if(rdx->user_connection_callback) rdx->user_connection_callback(rdx->connect_state);
}

void Redox::disconnected_callback(const redisAsyncContext *ctx, int status) {

  Redox* rdx = (Redox*) ctx->data;

  if (status != REDIS_OK) {
    cerr << "[ERROR] Disconnecting from Redis: " << ctx->errstr << endl;
    rdx->connect_state = REDOX_DISCONNECT_ERROR;
  } else {
    cout << "[INFO] Disconnected from Redis as planned." << endl;
    rdx->connect_state = REDOX_DISCONNECTED;
  }

  rdx->stop_signal();
  rdx->connect_waiter.notify_all();
  if(rdx->user_connection_callback) rdx->user_connection_callback(rdx->connect_state);
}

void Redox::init_ev() {
  signal(SIGPIPE, SIG_IGN);
  evloop = ev_loop_new(EVFLAG_AUTO);
  ev_set_userdata(evloop, (void*)this); // Back-reference
}

void Redox::init_hiredis() {

  ctx->data = (void*)this; // Back-reference

  if (ctx->err) {
    cout << "[ERROR] Could not create a hiredis context: " << ctx->errstr << endl;
    connect_state = REDOX_CONNECT_ERROR;
    connect_waiter.notify_all();
    return;
  }

  // Attach event loop to hiredis
  redisLibevAttach(evloop, ctx);

  // Set the callbacks to be invoked on server connection/disconnection
  redisAsyncSetConnectCallback(ctx, Redox::connected_callback);
  redisAsyncSetDisconnectCallback(ctx, Redox::disconnected_callback);
}

Redox::Redox(
  const string& host, const int port,
  std::function<void(int)> connection_callback
) : host(host), port(port), user_connection_callback(connection_callback) {

  init_ev();

  // Connect over TCP
  ctx = redisAsyncConnect(host.c_str(), port);

  init_hiredis();
}

Redox::Redox(
  const std::string& path,
  std::function<void(int)> connection_callback = nullptr
) : path(path), user_connection_callback(connection_callback) {

  init_ev();

  // Connect over unix sockets
  ctx = redisAsyncConnectUnix(path.c_str());

  init_hiredis();
}

void Redox::run_event_loop() {

  // Events to connect to Redox
  ev_run(evloop, EVRUN_NOWAIT);

  // Block until connected to Redis, or error
  unique_lock<mutex> ul(connect_lock);
  connect_waiter.wait(ul, [this] { return connect_state != REDOX_NOT_YET_CONNECTED; });

  // Handle connection error
  if(connect_state != REDOX_CONNECTED) {
    cout << "[INFO] Did not connect, event loop exiting." << endl;
    running_waiter.notify_one();
    return;
  }

  // Set up asynchronous watcher which we signal every
  // time we add a command
  ev_async_init(&async_w, process_queued_commands);
  ev_async_start(evloop, &async_w);

  running = true;
  running_waiter.notify_one();

  // Run the event loop
  while (!to_exit) {
    ev_run(evloop, EVRUN_NOWAIT);
  }

  cout << "[INFO] Stop signal detected." << endl;

  // Run a few more times to clear out canceled events
  for(int i = 0; i < 100; i++) {
    ev_run(evloop, EVRUN_NOWAIT);
  }

  if(commands_created != commands_deleted) {
    cerr << "[ERROR] All commands were not freed! "
         << commands_deleted << "/" << commands_created << endl;
  }

  exited = true;
  running = false;

  // Let go for block_until_stopped method
  exit_waiter.notify_one();

  cout << "[INFO] Event thread exited." << endl;
}

bool Redox::start() {

  event_loop_thread = thread([this] { run_event_loop(); });

  // Block until connected and running the event loop, or until
  // a connection error happens and the event loop exits
  unique_lock<mutex> ul(running_waiter_lock);
  running_waiter.wait(ul, [this] {
    return running.load() || connect_state == REDOX_CONNECT_ERROR;
  });

  // Return if succeeded
  return connect_state == REDOX_CONNECTED;
}

void Redox::stop_signal() {
  to_exit = true;
  ev_break(evloop, EVBREAK_ALL);
}

void Redox::block() {
  unique_lock<mutex> ul(exit_waiter_lock);
  exit_waiter.wait(ul, [this] { return exited.load(); });
}

void Redox::stop() {
  stop_signal();
  block();
}

void Redox::disconnect() {
  stop_signal();
  if(connect_state == REDOX_CONNECTED) {
    redisAsyncDisconnect(ctx);
    block();
  }
}

Redox::~Redox() {

  disconnect();

  if(event_loop_thread.joinable())
    event_loop_thread.join();

  ev_loop_destroy(evloop);

  std::cout << "[INFO] Redox created " << commands_created
    << " Commands and freed " << commands_deleted << "." << std::endl;
}

template<class ReplyT>
Command<ReplyT>* Redox::find_command(long id) {

  lock_guard<mutex> lg(command_map_guard);

  auto& command_map = get_command_map<ReplyT>();
  auto it = command_map.find(id);
  if(it == command_map.end()) return nullptr;
  return it->second;
}

template<class ReplyT>
void Redox::command_callback(redisAsyncContext *ctx, void *r, void *privdata) {

  Redox* rdx = (Redox*) ctx->data;
  long id = (long)privdata;
  redisReply* reply_obj = (redisReply*) r;

  Command<ReplyT>* c = rdx->find_command<ReplyT>(id);
  if(c == nullptr) {
//    cout << "[WARNING] Couldn't find Command " << id << " in command_map (command_callback)." << endl;
    freeReplyObject(reply_obj);
    return;
  }

  c->process_reply(reply_obj);

  // Increment the Redox object command counter
  rdx->cmd_count++;
}

/**
* Submit an asynchronous command to the Redox server. Return
* true if succeeded, false otherwise.
*/
template<class ReplyT>
bool Redox::submit_to_server(Command<ReplyT>* c) {
  c->pending++;
  if (redisAsyncCommand(c->rdx->ctx, command_callback<ReplyT>, (void*)c->id, c->cmd.c_str()) != REDIS_OK) {
    cerr << "[ERROR] Could not send \"" << c->cmd << "\": " << c->rdx->ctx->errstr << endl;
    c->invoke_error(REDOX_SEND_ERROR);
    return false;
  }

  return true;
}

template<class ReplyT>
void Redox::submit_command_callback(struct ev_loop* loop, ev_timer* timer, int revents) {

  Redox* rdx = (Redox*) ev_userdata(loop);
  long id = (long)timer->data;

  Command<ReplyT>* c = rdx->find_command<ReplyT>(id);
  if(c == nullptr) {
    cout << "[ERROR] Couldn't find Command " << id
         << " in command_map (submit_command_callback)." << endl;
    return;
  }

  if(c->is_canceled()) {

//    cout << "[INFO] Command " << c << " is completed, stopping event timer." << endl;

    c->timer_guard.lock();
    if((c->repeat != 0) || (c->after != 0))
      ev_timer_stop(loop, &c->timer);
    c->timer_guard.unlock();

    // Mark for memory to be freed when all callbacks are received
    c->timer.data = (void*)0;

    return;
  }

  submit_to_server<ReplyT>(c);
}

template<class ReplyT>
bool Redox::process_queued_command(long id) {

  Command<ReplyT>* c = find_command<ReplyT>(id);
  if(c == nullptr) return false;

  if((c->repeat == 0) && (c->after == 0)) {
    submit_to_server<ReplyT>(c);

  } else {

    c->timer.data = (void*)c->id;
    ev_timer_init(&c->timer, submit_command_callback<ReplyT>, c->after, c->repeat);
    ev_timer_start(evloop, &c->timer);

    c->timer_guard.unlock();
  }

  return true;
}

void Redox::process_queued_commands(struct ev_loop* loop, ev_async* async, int revents) {

  Redox* rdx = (Redox*) ev_userdata(loop);

  lock_guard<mutex> lg(rdx->queue_guard);

  while(!rdx->command_queue.empty()) {

    long id = rdx->command_queue.front();
    rdx->command_queue.pop();

    if(rdx->process_queued_command<redisReply*>(id)) {}
    else if(rdx->process_queued_command<std::string>(id)) {}
    else if(rdx->process_queued_command<char*>(id)) {}
    else if(rdx->process_queued_command<int>(id)) {}
    else if(rdx->process_queued_command<long long int>(id)) {}
    else if(rdx->process_queued_command<std::nullptr_t>(id)) {}
    else if(rdx->process_queued_command<std::vector<std::string>>(id)) {}
    else if(rdx->process_queued_command<std::set<std::string>>(id)) {}
    else if(rdx->process_queued_command<std::unordered_set<std::string>>(id)) {}
    else throw runtime_error("[FATAL] Command pointer not found in any queue!");
  }
}

// ---------------------------------
// get_command_map specializations
// ---------------------------------

template<> unordered_map<long, Command<redisReply*>*>&
Redox::get_command_map<redisReply*>() { return commands_redis_reply; }

template<> unordered_map<long, Command<string>*>&
Redox::get_command_map<string>() { return commands_string_r; }

template<> unordered_map<long, Command<char*>*>&
Redox::get_command_map<char*>() { return commands_char_p; }

template<> unordered_map<long, Command<int>*>&
Redox::get_command_map<int>() { return commands_int; }

template<> unordered_map<long, Command<long long int>*>&
Redox::get_command_map<long long int>() { return commands_long_long_int; }

template<> unordered_map<long, Command<nullptr_t>*>&
Redox::get_command_map<nullptr_t>() { return commands_null; }

template<> unordered_map<long, Command<vector<string>>*>&
Redox::get_command_map<vector<string>>() { return commands_vector_string; }

template<> unordered_map<long, Command<set<string>>*>&
Redox::get_command_map<set<string>>() { return commands_set_string; }

template<> unordered_map<long, Command<unordered_set<string>>*>&
Redox::get_command_map<unordered_set<string>>() { return commands_unordered_set_string; }

// ----------------------------
// Helpers
// ----------------------------

void Redox::command(const string& cmd) {
  command<redisReply*>(cmd);
}

bool Redox::command_blocking(const string& cmd) {
  Command<redisReply*>* c = command_blocking<redisReply*>(cmd);
  bool succeeded = c->ok();
  c->free();
  return succeeded;
}

string Redox::get(const string& key) {

  auto c = command_blocking<char*>("GET " + key);
  if(!c->ok()) {
    throw runtime_error("[FATAL] Error getting key " + key + ": Status code " + to_string(c->status()));
  }
  string reply = c->reply();
  c->free();
  return reply;
};

bool Redox::set(const std::string& key, const std::string& value) {
  return command_blocking("SET " + key + " " + value);
}

bool Redox::del(const std::string& key) {
  return command_blocking("DEL " + key);
}

} // End namespace redis
