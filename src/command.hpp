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

class Redox;
//class Command;

//template <typename ReplyT>
//using CallbackT = std::function<void(Command<ReplyT>&)>;

/**
* The Command class represents a single command string to be sent to
* a Redis server, for both synchronous and asynchronous usage. It manages
* all of the state relevant to a single command string. A Command can also
* represent a deferred or looping command, in which case the success or
* error callbacks are invoked more than once.
*/
template<class ReplyT>
class Command {

public:

  // Reply codes
  static const int NO_REPLY = -1; // No reply yet
  static const int OK_REPLY = 0; // Successful reply of the expected type
  static const int NIL_REPLY = 1; // Got a nil reply
  static const int ERROR_REPLY = 2; // Got an error reply
  static const int SEND_ERROR = 3; // Could not send to server
  static const int WRONG_TYPE = 4; // Got reply, but it was not the expected type
  static const int TIMEOUT = 5; // No reply, timed out

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
  bool canceled() const { return canceled_; }

  /**
  * Returns the reply status of this command.
  * Use ONLY with command_blocking.
  */
  int status() const { return reply_status_; };

  /**
  * Returns true if this command got a successful reply.
  * Use ONLY with command_blocking.
  */
  bool ok() const { return reply_status_ == OK_REPLY; }

  /**
  * Returns the reply value, if the reply was successful (ok() == true).
  * Use ONLY with command_blocking.
  */
  const ReplyT& reply() const;

  const std::string& cmd() const { return cmd_; };

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
      const std::function<void(Command<ReplyT>&)>& callback,
      double repeat, double after,
      bool free_memory,
      log::Logger& logger
  );

  // Handles a new reply from the server
  void processReply(redisReply* r);

  // Invoke a user callback from the reply object. This method is specialized
  // for each ReplyT of Command.
  void parseReplyObject();

  // Directly invoke the user callback if it exists
  void invoke() { if(callback_) callback_(*this); }

  bool checkErrorReply();
  bool checkNilReply();
  bool isExpectedReply(int type);
  bool isExpectedReply(int typeA, int typeB);

  // Delete the provided Command object and deregister as an active
  // command from its Redox instance.
  static void freeCommand(Command<ReplyT>* c);

  // If needed, free the redisReply
  void freeReply();

  // The last server reply
  redisReply* reply_obj_ = nullptr;

  // User callback
  const std::function<void(Command<ReplyT>&)> callback_;

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
