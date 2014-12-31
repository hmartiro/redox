/**
* Redis C++11 wrapper.
*/

#pragma once

#include <iostream>
#include <functional>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include <string>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libev.h>

#include "command.hpp"

namespace redox {

// Default to a local Redis server
static const std::string REDIS_DEFAULT_HOST = "localhost";
static const int REDIS_DEFAULT_PORT = 6379;

class Redox {

public:

  /**
  * Initialize everything, connect over TCP to a Redis server.
  */
  Redox(const std::string& host = REDIS_DEFAULT_HOST, const int port = REDIS_DEFAULT_PORT);
  ~Redox();

  /**
  * Connect to Redis and start the event loop in a separate thread. Returns
  * once everything is ready to go.
  */
  void start();

  /**
  * Signal the event loop to stop processing commands and shut down.
  */
  void stop_signal();

  /**
  * Wait for the event loop to exit, then return.
  */
  void block();

  /**
  * Signal the event loop to stop, wait for all pending commands to be processed,
  * and shut everything down. A simple combination of stop_signal() and block().
  */
  void stop();

  /**
  * Create an asynchronous Redis command to be executed. Return a pointer to a
  * Command object that represents this command. If the command succeeded, the
  * callback is invoked with a reference to the reply. If something went wrong,
  * the error_callback is invoked with an error_code. One of the two is guaranteed
  * to be invoked. The method is templated by the expected data type of the reply,
  * and can be one of {redisReply*, string, char*, int, long long int, nullptr_t}.
  *
  *            cmd: The command to be run.
  *       callback: A function invoked on a successful reply from the server.
  * error_callback: A function invoked on some error state.
  *         repeat: If non-zero, executes the command continuously at the given rate
  *                 in seconds, until cancel() is called on the Command object.
  *          after: If non-zero, executes the command after the given delay in seconds.
  *    free_memory: If true (default), Redox automatically frees the Command object and
  *                 reply from the server after a callback is invoked. If false, the
  *                 user is responsible for calling free() on the Command object.
  */
  template<class ReplyT>
  Command<ReplyT>* command(
    const std::string& cmd,
    const std::function<void(const std::string&, const ReplyT&)>& callback = nullptr,
    const std::function<void(const std::string&, int status)>& error_callback = nullptr,
    double repeat = 0.0,
    double after = 0.0,
    bool free_memory = true
  );

  /**
  * A wrapper around command() for synchronous use. Waits for a reply, populates it
  * into the Command object, and returns when complete. The user can retrieve the
  * results from the Command object - ok() will tell you if the call succeeded,
  * status() will give the error code, and reply() will return the reply data if
  * the call succeeded.
  */
  template<class ReplyT>
  Command<ReplyT>* command_blocking(const std::string& cmd);

  /**
  * Return the total number of successful commands processed by this Redox instance.
  */
  long num_commands_processed() { return cmd_count; }

  // Hiredis context, left public to allow low-level access
  redisAsyncContext *ctx;

  // ------------------------------------------------
  // Wrapper methods for convenience only
  // ------------------------------------------------

  /**
  * Non-templated version of command in case you really don't care
  * about the reply and just want to send something off.
  */
  void command(const std::string& command);

  /**
  * Non-templated version of command_blocking in case you really don't
  * care about the reply. Returns true if succeeded, false if error.
  */
  bool command_blocking(const std::string& command);

  /**
  * Redis GET command wrapper - return the value for the given key, or throw
  * an exception if there is an error. Blocking call, of course.
  */
  std::string get(const std::string& key);

  /**
  * Redis SET command wrapper - set the value for the given key. Return
  * true if succeeded, false if error.
  */
  bool set(const std::string& key, const std::string& value);

  /**
  * Redis DEL command wrapper - delete the given key. Return true if succeeded,
  * false if error.
  */
  bool del(const std::string& key);

  // TODO pub/sub
//  void publish(std::string channel, std::string msg);
//  void subscribe(std::string channel, std::function<void(std::string channel, std::string msg)> callback);
//  void unsubscribe(std::string channel);

  // Invoked by Command objects when they are completed
  template<class ReplyT>
  void remove_active_command(const long id) {
    std::lock_guard<std::mutex> lg1(command_map_guard);
    get_command_map<ReplyT>().erase(id);
    commands_deleted += 1;
  }

private:

  // Redox server
  std::string host;
  int port;

  // Block run() until redis is connected
  std::mutex connected_lock;

  // Dynamically allocated libev event loop
  struct ev_loop* evloop;

  // Number of commands processed
  std::atomic_long cmd_count = {0};

  // Track of Command objects allocated. Also provides unique Command IDs.
  std::atomic_long commands_created = {0};
  std::atomic_long commands_deleted = {0};

  // Separate thread to have a non-blocking event loop
  std::thread event_loop_thread;

  // Variable and CV to know when the event loop starts running
  std::atomic_bool running = {false};
  std::mutex running_waiter_lock;
  std::condition_variable running_waiter;

  // Variable and CV to know when the event loop stops running
  std::atomic_bool to_exit = {false}; // Signal to exit
  std::atomic_bool exited = {false}; // Event thread exited
  std::mutex exit_waiter_lock;
  std::condition_variable exit_waiter;

  // Maps of each Command, fetchable by the unique ID number
  std::unordered_map<long, Command<redisReply*>*> commands_redis_reply;
  std::unordered_map<long, Command<std::string>*> commands_string_r;
  std::unordered_map<long, Command<char*>*> commands_char_p;
  std::unordered_map<long, Command<int>*> commands_int;
  std::unordered_map<long, Command<long long int>*> commands_long_long_int;
  std::unordered_map<long, Command<std::nullptr_t>*> commands_null;
  std::unordered_map<long, Command<std::vector<std::string>>*> commands_vector_string;
  std::mutex command_map_guard; // Guards access to all of the above

  // Return the correct map from the above, based on the template specialization
  template<class ReplyT>
  std::unordered_map<long, Command<ReplyT>*>& get_command_map();

  // Return the given Command from the relevant command map, or nullptr if not there
  template<class ReplyT>
  Command<ReplyT>* find_command(long id);

  std::queue<long> command_queue;
  std::mutex queue_guard;
  void process_queued_commands();

  template<class ReplyT>
  bool process_queued_command(long id);

  void run_event_loop();

  // Callbacks invoked on server connection/disconnection
  static void connected(const redisAsyncContext *c, int status);
  static void disconnected(const redisAsyncContext *c, int status);

  template<class ReplyT>
  static void command_callback(redisAsyncContext *ctx, void *r, void *privdata);

  template<class ReplyT>
  static bool submit_to_server(Command<ReplyT>* c);

  template<class ReplyT>
  static void submit_command_callback(struct ev_loop* loop, ev_timer* timer, int revents);
};

// ---------------------------

template<class ReplyT>
Command<ReplyT>* Redox::command(
  const std::string& cmd,
  const std::function<void(const std::string&, const ReplyT&)>& callback,
  const std::function<void(const std::string&, int status)>& error_callback,
  double repeat,
  double after,
  bool free_memory
) {

  if(!running) {
    throw std::runtime_error("[ERROR] Need to start Redox before running commands!");
  }

  commands_created += 1;
  auto* c = new Command<ReplyT>(this, commands_created, cmd,
    callback, error_callback, repeat, after, free_memory);

  std::lock_guard<std::mutex> lg(queue_guard);
  std::lock_guard<std::mutex> lg2(command_map_guard);

  get_command_map<ReplyT>()[c->id] = c;
  command_queue.push(c->id);

//  std::cout << "[DEBUG] Created Command " << c->id << " at " << c << std::endl;

  return c;
}

template<class ReplyT>
Command<ReplyT>* Redox::command_blocking(const std::string& cmd) {

  ReplyT val;
  std::atomic_int status(REDOX_UNINIT);

  std::condition_variable cv;
  std::mutex m;

  std::unique_lock<std::mutex> lk(m);

  Command<ReplyT>* c = command<ReplyT>(cmd,
    [&val, &status, &m, &cv](const std::string& cmd_str, const ReplyT& reply) {
      std::unique_lock<std::mutex> ul(m);
      val = reply;
      status = REDOX_OK;
      ul.unlock();
      cv.notify_one();
    },
    [&status, &m, &cv](const std::string& cmd_str, int error) {
      std::unique_lock<std::mutex> ul(m);
      status = error;
      ul.unlock();
      cv.notify_one();
    },
    0, 0, false // No repeats, don't free memory
  );

  // Wait until a callback is invoked
  cv.wait(lk, [&status] { return status != REDOX_UNINIT; });

  c->reply_val = val;
  c->reply_status = status;

  return c;
}

} // End namespace redis
