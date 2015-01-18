redox
======

Modern, asynchronous, and wicked fast C++11 bindings for Redis

## Overview

Redox provides an elegant high-level interface to Redis that makes it easy to write
high-performance applications. It is built on top of [hiredis](https://github.com/redis/hiredis/)
(but uses only its asynchronous API, even for synchronous calls) and
[libev](http://manpages.ubuntu.com/manpages/raring/man3/ev.3.html).

 * A single `command()` method, templated by the user based on the expected return type
 * Callbacks are [std::functions](http://en.cppreference.com/w/cpp/utility/functional/function),
   so they can capture state - lambdas, member functions, bind expressions
 * Thread-safe - use one Redox object in multiple threads or multiple Redox objects in one thread
 * Automatic pipelining, even for synchronous calls from separate threads
 * Small, < 1k SLOC
 * Access to low-level reply objects when needed
 * 100% clean Valgrind reports

## Performance Benchmarks
Benchmarks are given by averaging the results of five trials of various programs
in `examples/`. The results are on an AWS t2.medium instance running Ubuntu 14.04 (64-bit).
During these tests, Redox communicated with a local Redis server over TCP.

 * 100 repeating asynchronous commands (`speed_test_async_multi`): **710,014 commands/s**
 * One repeating asynchronous command (`speed_test_async`): **195,159 commands/s**
 * One blocking command in a loop (`speed_test_sync`): **23,609 commands/s**

## Build from source
Instructions provided are for Ubuntu, but Redox is fully platform-independent.

Get the build environment:

    sudo apt-get install git cmake build-essential

Get the dependencies:

    sudo apt-get install libhiredis-dev libev-dev

Build Redox and examples using CMake (a helper script is provided):

    cd redox
    ./make.sh

## Tutorial
Coming soon. For now, look at the example programs located in `examples/`, and the snippets
posted below.

Basic synchronous usage:

    redox::Redox rdx = {"localhost", 6379}; // Initialize Redox
    rdx.start(); // Start the event loop
    
    rdx.del("occupation");
    
    if(!rdx.set("occupation", "carpenter")) // Set a key, check if succeeded
      cerr << "Failed to set key!" << endl;
    
    cout << "key = occupation, value = \"" << rdx.get("occupation") << "\"" << endl;
    
    rdx.stop(); // Shut down the event loop

Output: `key = occupation, value = "carpenter"`

The `command` method launches a command asynchronously. This is the root
of every method in Redox that executes a command:

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

The `command_blocking` method is the root of all synchronous calls. It calls `command` then
uses a condition variable to wait for a reply.

    /**
    * A wrapper around command() for synchronous use. Waits for a reply, populates it
    * into the Command object, and returns when complete. The user can retrieve the
    * results from the Command object - ok() will tell you if the call succeeded,
    * status() will give the error code, and reply() will return the reply data if
    * the call succeeded.
    */
    template<class ReplyT>
    Command<ReplyT>* command_blocking(const std::string& cmd);

