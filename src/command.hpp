/**
* Redis C++11 wrapper.
*/

#pragma once

#include <iostream>
#include <string>
#include <functional>
#include <atomic>
#include <mutex>

#include <hiredis/adapters/libev.h>
#include <hiredis/async.h>

#include "utils/logger.hpp"

namespace redox {

static const int REDOX_UNINIT = -1;
static const int REDOX_OK = 0;
static const int REDOX_SEND_ERROR = 1;
static const int REDOX_WRONG_TYPE = 2;
static const int REDOX_NIL_REPLY = 3;
static const int REDOX_ERROR_REPLY = 4;
static const int REDOX_TIMEOUT = 5;

class Redox;

template<class ReplyT>
class Command {

friend class Redox;

public:
  Command(
    Redox* rdx,
    long id,
    const std::string& cmd,
    const std::function<void(const std::string&, const ReplyT&)>& callback,
    const std::function<void(const std::string&, int status)>& error_callback,
    double repeat, double after,
    bool free_memory,
    log::Logger& logger
  );

  Redox* rdx;

  const long id;
  const std::string cmd;
  const double repeat;
  const double after;

  const bool free_memory;

  redisReply* reply_obj = nullptr;

  std::atomic_int pending = {0};

  void invoke(const ReplyT& reply);
  void invoke_error(int status);

  const ReplyT& reply();
  int status() { return reply_status; };
  bool ok() { return reply_status == REDOX_OK; }
  bool is_canceled() { return canceled; }

  void cancel() { canceled = true; }

  /**
  * Called by the user to free the redisReply object, when the free_memory
  * flag is set to false for a command.
  */
  void free();

  void process_reply(redisReply* r);

  ev_timer* get_timer() {
    std::lock_guard<std::mutex> lg(timer_guard);
    return &timer;
  }

  static void free_command(Command<ReplyT>* c);

private:

  const std::function<void(const std::string&, const ReplyT&)> callback;
  const std::function<void(const std::string&, int status)> error_callback;

  // Place to store the reply value and status.
  // ONLY for blocking commands
  ReplyT reply_val;
  int reply_status;

  std::atomic_bool canceled = {false};

  ev_timer timer;
  std::mutex timer_guard;

  // Make sure we don't free resources until details taken care of
  std::mutex free_guard;

  void free_reply_object();

  void invoke_callback();
  bool is_error_reply();
  bool is_nil_reply();

  log::Logger& logger;
};

template<class ReplyT>
Command<ReplyT>::Command(
    Redox* rdx,
    long id,
    const std::string& cmd,
    const std::function<void(const std::string&, const ReplyT&)>& callback,
    const std::function<void(const std::string&, int status)>& error_callback,
    double repeat, double after, bool free_memory, log::Logger& logger
) : rdx(rdx), id(id), cmd(cmd), repeat(repeat), after(after), free_memory(free_memory),
    callback(callback), error_callback(error_callback), logger(logger)
{
  timer_guard.lock();
}

template<class ReplyT>
void Command<ReplyT>::process_reply(redisReply* r) {

  free_guard.lock();

  reply_obj = r;
  invoke_callback();

  pending--;

  // Allow free() method to free memory
  if(!free_memory) {
//    logger.trace() << "Command memory not being freed, free_memory = " << free_memory;
    free_guard.unlock();
    return;
  }

  free_reply_object();

  // Handle memory if all pending replies have arrived
  if(pending == 0) {

    // Just free non-repeating commands
    if (repeat == 0) {
      free_command(this);
      return;

    // Free repeating commands if timer is stopped
    } else {
      if((long)(get_timer()->data) == 0) {
        free_command(this);
        return;
      }
    }
  }

  free_guard.unlock();
}

template<class ReplyT>
void Command<
  ReplyT>::invoke(const ReplyT& r) {
  if(callback) callback(cmd, r);
}

template<class ReplyT>
void Command<ReplyT>::invoke_error(int status) {
  if(error_callback) error_callback(cmd, status);
}

template<class ReplyT>
void Command<ReplyT>::free_reply_object() {

  if(reply_obj == nullptr) {
    logger.error() << cmd << ": Attempting to double free reply object.";
    return;
  }

  freeReplyObject(reply_obj);
  reply_obj = nullptr;
}

template<class ReplyT>
void Command<ReplyT>::free_command(Command<ReplyT>* c) {
  c->rdx->template remove_active_command<ReplyT>(c->id);
//  logger.debug() << "Deleted Command " << c->id << " at " << c;
  delete c;
}

template<class ReplyT>
void Command<ReplyT>::free() {

  free_guard.lock();
  free_reply_object();
  free_guard.unlock();

  free_command(this);
}

template<class ReplyT>
const ReplyT& Command<ReplyT>::reply() {
  if(!ok()) {
    logger.warning() << cmd << ": Accessing value of reply with status != OK.";
  }
  return reply_val;
}

} // End namespace redis
