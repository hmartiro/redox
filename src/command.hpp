/**
* Redis C++11 wrapper.
*/

#pragma once

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

public:

  /**
  * Frees memory allocated by this command. Commands with free_memory = false
  * must be freed by the user.
  */
  void free();

  /**
  * Cancels a repeating or delayed command.
  */
  void cancel() { canceled_ = true; }

  /**
  * Returns true if the command has been canceled.
  */
  bool canceled() { return canceled_; }

  /**
  * Returns the reply status of this command.
  * Use ONLY with command_blocking.
  */
  int status() { return reply_status_; };

  /**
  * Returns true if this command got a successful reply.
  * Use ONLY with command_blocking.
  */
  bool ok() { return reply_status_ == REDOX_OK; }

  /**
  * Returns the reply value, if the reply was successful (ok() == true).
  * Use ONLY with command_blocking.
  */
  const ReplyT& reply();

  // Allow public access to constructed data
  Redox* const rdx_;
  const long id_;
  const std::string cmd_;
  const double repeat_;
  const double after_;
  const bool free_memory_;

private:

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

  // Handles a new reply from the server
  void processReply(redisReply* r);

  // Invoke a user callback from the reply object. This method is specialized
  // for each ReplyT of Command.
  void handleCallback();

  // Directly invoke the user callbacks if the exist
  void invokeSuccess(const ReplyT& reply) { if (success_callback_) success_callback_(cmd_, reply); }
  void invokeError(int status) { if (error_callback_) error_callback_(cmd_, status); }

  bool isErrorReply();
  bool isNilReply();

  // Delete the provided Command object and deregister as an active
  // command from its Redox instance.
  static void freeCommand(Command<ReplyT>* c);

  // If needed, free the redisReply
  void freeReply();

  // The last server reply
  redisReply* reply_obj_ = nullptr;

  // Callbacks on success and error
  const std::function<void(const std::string&, const ReplyT&)> success_callback_;
  const std::function<void(const std::string&, int status)> error_callback_;

  // Place to store the reply value and status.
  // ONLY for blocking commands
  ReplyT reply_val_;
  int reply_status_;

  // How many messages sent to server but not received reply
  std::atomic_int pending_ = {0};

  // Whether a repeating or delayed command is canceled
  std::atomic_bool canceled_ = {false};

  // libev timer watcher
  ev_timer timer_;
  std::mutex timer_guard_;

  // Make sure we don't free resources until details taken care of
  std::mutex free_guard_;

  // Passed on from Redox class
  log::Logger& logger_;

  friend class Redox;
};

} // End namespace redis
