/**
* Redox - A modern, asynchronous, and wicked fast C++11 client for Redis
*
*    https://github.com/hmartiro/redox
*
* Copyright 2015 - Hayk Martirosyan <hayk.mart at gmail dot com>
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*    http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include <signal.h>
#include "redox.hpp"

using namespace std;

namespace redox {

void Redox::connectedCallback(const redisAsyncContext* ctx, int status) {

  Redox* rdx = (Redox*) ctx->data;

  if (status != REDIS_OK) {
    rdx->logger_.fatal() << "Could not connect to Redis: " << ctx->errstr;
    rdx->connect_state_ = CONNECT_ERROR;

  } else {
    // Disable hiredis automatically freeing reply objects
    ctx->c.reader->fn->freeObject = [](void *reply) {};
    rdx->connect_state_ = CONNECTED;
    rdx->logger_.info() << "Connected to Redis.";
  }

  rdx->connect_waiter_.notify_all();
  if(rdx->user_connection_callback_) rdx->user_connection_callback_(rdx->connect_state_);
}

void Redox::disconnectedCallback(const redisAsyncContext* ctx, int status) {

  Redox* rdx = (Redox*) ctx->data;

  if (status != REDIS_OK) {
    rdx->logger_.error() << "Could not disconnect from Redis: " << ctx->errstr;
    rdx->connect_state_ = DISCONNECT_ERROR;
  } else {
    rdx->logger_.info() << "Disconnected from Redis as planned.";
    rdx->connect_state_ = DISCONNECTED;
  }

  rdx->stop();
  rdx->connect_waiter_.notify_all();
  if(rdx->user_connection_callback_) rdx->user_connection_callback_(rdx->connect_state_);
}

void Redox::init_ev() {
  signal(SIGPIPE, SIG_IGN);
  evloop_ = ev_loop_new(EVFLAG_AUTO);
  ev_set_userdata(evloop_, (void*)this); // Back-reference
}

void Redox::init_hiredis() {

  ctx_->data = (void*)this; // Back-reference

  if (ctx_->err) {
    logger_.error() << "Could not create a hiredis context: " << ctx_->errstr;
    connect_state_ = CONNECT_ERROR;
    connect_waiter_.notify_all();
    return;
  }

  // Attach event loop to hiredis
  redisLibevAttach(evloop_, ctx_);

  // Set the callbacks to be invoked on server connection/disconnection
  redisAsyncSetConnectCallback(ctx_, Redox::connectedCallback);
  redisAsyncSetDisconnectCallback(ctx_, Redox::disconnectedCallback);
}

Redox::Redox(
  const string& host, const int port,
  function<void(int)> connection_callback,
  ostream& log_stream,
  log::Level log_level
) : host_(host), port_(port),
    logger_(log_stream, log_level),
    user_connection_callback_(connection_callback) {

  init_ev();

  // Connect over TCP
  ctx_ = redisAsyncConnect(host.c_str(), port);

  init_hiredis();
}

Redox::Redox(
  const string& path,
  function<void(int)> connection_callback,
  ostream& log_stream,
  log::Level log_level
) : host_(), port_(), path_(path), logger_(log_stream, log_level),
    user_connection_callback_(connection_callback) {

  init_ev();

  // Connect over unix sockets
  ctx_ = redisAsyncConnectUnix(path.c_str());

  init_hiredis();
}

void breakEventLoop(struct ev_loop* loop, ev_async* async, int revents) {
  ev_break(loop, EVBREAK_ALL);
}

void Redox::runEventLoop() {

  // Events to connect to Redox
  ev_run(evloop_, EVRUN_NOWAIT);

  // Block until connected to Redis, or error
  unique_lock<mutex> ul(connect_lock_);
  connect_waiter_.wait(ul, [this] { return connect_state_ != NOT_YET_CONNECTED; });

  // Handle connection error
  if(connect_state_ != CONNECTED) {
    logger_.warning() << "Did not connect, event loop exiting.";
    running_waiter_.notify_one();
    return;
  }

  // Set up asynchronous watcher which we signal every
  // time we add a command
  ev_async_init(&watcher_command_, processQueuedCommands);
  ev_async_start(evloop_, &watcher_command_);

  // Set up an async watcher to break the loop
  ev_async_init(&watcher_stop_, breakEventLoop);
  ev_async_start(evloop_, &watcher_stop_);

  // Set up an async watcher which we signal every time
  // we want a command freed
  ev_async_init(&watcher_free_, freeQueuedCommands);
  ev_async_start(evloop_, &watcher_free_);

  running_ = true;
  running_waiter_.notify_one();

  // Run the event loop
  // TODO this hogs resources, but ev_run(evloop_) without
  // the manual loop is slower. Maybe use a CV to run sparsely
  // unless there are commands to process?
  while (!to_exit_) {
    ev_run(evloop_, EVRUN_NOWAIT);
  }

  logger_.info() << "Stop signal detected. Closing down event loop.";

  // Signal event loop to free all commands
  freeAllCommands();

  // Wait to receive server replies for clean hiredis disconnect
  this_thread::sleep_for(chrono::milliseconds(10));
  ev_run(evloop_, EVRUN_NOWAIT);

  if(connect_state_ == CONNECTED) redisAsyncDisconnect(ctx_);

  // Run once more to disconnect
  ev_run(evloop_, EVRUN_NOWAIT);

  if(commands_created_ != commands_deleted_) {
    logger_.error() << "All commands were not freed! "
         << commands_deleted_ << "/" << commands_created_;
  }

  exited_ = true;
  running_ = false;

  // Let go for block_until_stopped method
  exit_waiter_.notify_one();

  logger_.info() << "Event thread exited.";
}

bool Redox::connect() {

  event_loop_thread_ = thread([this] { runEventLoop(); });

  // Block until connected and running the event loop, or until
  // a connection error happens and the event loop exits
  unique_lock<mutex> ul(running_waiter_lock_);
  running_waiter_.wait(ul, [this] {
    return running_.load() || connect_state_ == CONNECT_ERROR;
  });

  // Return if succeeded
  return connect_state_ == CONNECTED;
}

void Redox::disconnect() {
  stop();
  wait();
}

void Redox::stop() {
  to_exit_ = true;
  logger_.debug() << "stop() called, breaking event loop";
  ev_async_send(evloop_, &watcher_stop_);
}

void Redox::wait() {
  unique_lock<mutex> ul(exit_waiter_lock_);
  exit_waiter_.wait(ul, [this] { return exited_.load(); });
}

Redox::~Redox() {

  // Bring down the event loop
  stop();

  if(event_loop_thread_.joinable()) event_loop_thread_.join();
  ev_loop_destroy(evloop_);
}

template<class ReplyT>
Command<ReplyT>* Redox::findCommand(long id) {

  lock_guard<mutex> lg(command_map_guard_);

  auto& command_map = getCommandMap<ReplyT>();
  auto it = command_map.find(id);
  if(it == command_map.end()) return nullptr;
  return it->second;
}

template<class ReplyT>
void Redox::commandCallback(redisAsyncContext* ctx, void* r, void* privdata) {

  Redox* rdx = (Redox*) ctx->data;
  long id = (long)privdata;
  redisReply* reply_obj = (redisReply*) r;

  Command<ReplyT>* c = rdx->findCommand<ReplyT>(id);
  if(c == nullptr) {
//    rdx->logger.warning() << "Couldn't find Command " << id << " in command_map (commandCallback).";
    freeReplyObject(reply_obj);
    return;
  }

  c->processReply(reply_obj);
}

template<class ReplyT>
bool Redox::submitToServer(Command<ReplyT>* c) {

  Redox* rdx = c->rdx_;
  c->pending_++;

  // Process binary data if trailing quotation. This is a limited implementation
  // to allow binary data between the first and the last quotes of the command string,
  // if the very last character of the command is a quote ('"').
  if(c->cmd_[c->cmd_.size()-1] == '"') {

    // Indices of the quotes
    size_t first = c->cmd_.find('"');
    size_t last = c->cmd_.size()-1;

    // Proceed only if the first and last quotes are different
    if(first != last) {

      string format = c->cmd_.substr(0, first) + "%b";
      string value = c->cmd_.substr(first+1, last-first-1);
      if (redisAsyncCommand(rdx->ctx_, commandCallback<ReplyT>, (void*) c->id_, format.c_str(), value.c_str(), value.size()) != REDIS_OK) {
        rdx->logger_.error() << "Could not send \"" << c->cmd_ << "\": " << rdx->ctx_->errstr;
        c->reply_status_ = Command<ReplyT>::SEND_ERROR;
        c->invoke();
        return false;
      }
      return true;
    }
  }

  if (redisAsyncCommand(rdx->ctx_, commandCallback<ReplyT>, (void*) c->id_, c->cmd_.c_str()) != REDIS_OK) {
    rdx->logger_.error() << "Could not send \"" << c->cmd_ << "\": " << rdx->ctx_->errstr;
    c->reply_status_ = Command<ReplyT>::SEND_ERROR;
    c->invoke();
    return false;
  }

  return true;
}

template<class ReplyT>
void Redox::submitCommandCallback(struct ev_loop* loop, ev_timer* timer, int revents) {

  Redox* rdx = (Redox*) ev_userdata(loop);
  long id = (long)timer->data;

  Command<ReplyT>* c = rdx->findCommand<ReplyT>(id);
  if(c == nullptr) {
    rdx->logger_.error() << "Couldn't find Command " << id
         << " in command_map (submitCommandCallback).";
    return;
  }

  submitToServer<ReplyT>(c);
}

template<class ReplyT>
bool Redox::processQueuedCommand(long id) {

  Command<ReplyT>* c = findCommand<ReplyT>(id);
  if(c == nullptr) return false;

  if((c->repeat_ == 0) && (c->after_ == 0)) {
    submitToServer<ReplyT>(c);

  } else {

    c->timer_.data = (void*)c->id_;
    ev_timer_init(&c->timer_, submitCommandCallback<ReplyT>, c->after_, c->repeat_);
    ev_timer_start(evloop_, &c->timer_);

    c->timer_guard_.unlock();
  }

  return true;
}

void Redox::processQueuedCommands(struct ev_loop* loop, ev_async* async, int revents) {

  Redox* rdx = (Redox*) ev_userdata(loop);

  lock_guard<mutex> lg(rdx->queue_guard_);

  while(!rdx->command_queue_.empty()) {

    long id = rdx->command_queue_.front();
    rdx->command_queue_.pop();

    if(rdx->processQueuedCommand<redisReply*>(id)) {}
    else if(rdx->processQueuedCommand<string>(id)) {}
    else if(rdx->processQueuedCommand<char*>(id)) {}
    else if(rdx->processQueuedCommand<int>(id)) {}
    else if(rdx->processQueuedCommand<long long int>(id)) {}
    else if(rdx->processQueuedCommand<nullptr_t>(id)) {}
    else if(rdx->processQueuedCommand<vector<string>>(id)) {}
    else if(rdx->processQueuedCommand<std::set<string>>(id)) {}
    else if(rdx->processQueuedCommand<unordered_set<string>>(id)) {}
    else throw runtime_error("Command pointer not found in any queue!");
  }
}

void Redox::freeQueuedCommands(struct ev_loop* loop, ev_async* async, int revents) {

  Redox* rdx = (Redox*) ev_userdata(loop);

  lock_guard<mutex> lg(rdx->free_queue_guard_);

  while(!rdx->commands_to_free_.empty()) {
    long id = rdx->commands_to_free_.front();
    rdx->commands_to_free_.pop();

    if(rdx->freeQueuedCommand<redisReply*>(id)) {}
    else if(rdx->freeQueuedCommand<string>(id)) {}
    else if(rdx->freeQueuedCommand<char*>(id)) {}
    else if(rdx->freeQueuedCommand<int>(id)) {}
    else if(rdx->freeQueuedCommand<long long int>(id)) {}
    else if(rdx->freeQueuedCommand<nullptr_t>(id)) {}
    else if(rdx->freeQueuedCommand<vector<string>>(id)) {}
    else if(rdx->freeQueuedCommand<std::set<string>>(id)) {}
    else if(rdx->freeQueuedCommand<unordered_set<string>>(id)) {}
    else {}
  }
}

template<class ReplyT>
bool Redox::freeQueuedCommand(long id) {
  Command<ReplyT>* c = findCommand<ReplyT>(id);
  if(c == nullptr) return false;

  c->freeReply();

  // Stop the libev timer if this is a repeating command
  if((c->repeat_ != 0) || (c->after_ != 0)) {
    lock_guard<mutex> lg(c->timer_guard_);
    ev_timer_stop(c->rdx_->evloop_, &c->timer_);
  }

  deregisterCommand<ReplyT>(c->id_);

  delete c;

  return true;
}

long Redox::freeAllCommands() {
  return freeAllCommandsOfType<redisReply*>() +
      freeAllCommandsOfType<string>() +
      freeAllCommandsOfType<char*>() +
      freeAllCommandsOfType<int>() +
      freeAllCommandsOfType<long long int>() +
      freeAllCommandsOfType<nullptr_t>() +
      freeAllCommandsOfType<vector<string>>() +
      freeAllCommandsOfType<std::set<string>>() +
      freeAllCommandsOfType<unordered_set<string>>();
}

template<class ReplyT>
long Redox::freeAllCommandsOfType() {

  lock_guard<mutex> lg(free_queue_guard_);
  lock_guard<mutex> lg2(queue_guard_);

  auto& command_map = getCommandMap<ReplyT>();
  long len = command_map.size();

  for(auto& pair : command_map) {
    Command<ReplyT>* c = pair.second;

    c->freeReply();

    // Stop the libev timer if this is a repeating command
    if((c->repeat_ != 0) || (c->after_ != 0)) {
      lock_guard<mutex> lg3(c->timer_guard_);
      ev_timer_stop(c->rdx_->evloop_, &c->timer_);
    }

    delete c;
  }

  command_map.clear();
  commands_deleted_ += len;

  return len;
}

// ---------------------------------
// get_command_map specializations
// ---------------------------------

template<> unordered_map<long, Command<redisReply*>*>&
Redox::getCommandMap<redisReply*>() { return commands_redis_reply_; }

template<> unordered_map<long, Command<string>*>&
Redox::getCommandMap<string>() { return commands_string_; }

template<> unordered_map<long, Command<char*>*>&
Redox::getCommandMap<char*>() { return commands_char_p_; }

template<> unordered_map<long, Command<int>*>&
Redox::getCommandMap<int>() { return commands_int_; }

template<> unordered_map<long, Command<long long int>*>&
Redox::getCommandMap<long long int>() { return commands_long_long_int_; }

template<> unordered_map<long, Command<nullptr_t>*>&
Redox::getCommandMap<nullptr_t>() { return commands_null_; }

template<> unordered_map<long, Command<vector<string>>*>&
Redox::getCommandMap<vector<string>>() { return commands_vector_string_; }

template<> unordered_map<long, Command<set<string>>*>&
Redox::getCommandMap<set<string>>() { return commands_set_string_; }

template<> unordered_map<long, Command<unordered_set<string>>*>&
Redox::getCommandMap<unordered_set<string>>() { return commands_unordered_set_string_; }

// ----------------------------
// Helpers
// ----------------------------

void Redox::command(const std::string& cmd) {
  command<redisReply*>(cmd, nullptr);
}

bool Redox::commandSync(const string& cmd) {
  auto& c = commandSync<redisReply*>(cmd);
  bool succeeded = c.ok();
  c.free();
  return succeeded;
}

string Redox::get(const string& key) {

  Command<char*>& c = commandSync<char*>("GET " + key);
  if(!c.ok()) {
    throw runtime_error("[FATAL] Error getting key " + key + ": Status code " + to_string(c.status()));
  }
  string reply = c.reply();
  c.free();
  return reply;
};

bool Redox::set(const string& key, const string& value) {
  return commandSync("SET " + key + " " + value);
}

bool Redox::del(const string& key) {
  return commandSync("DEL " + key);
}

void Redox::publish(const string& topic, const string& msg) {
  command<redisReply*>("PUBLISH " + topic + " " + msg);
}

} // End namespace redis
