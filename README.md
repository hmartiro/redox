redox
======
```diff
- ANNOUNCEMENT from hayk (@hmartiro): Unfortunately, I no longer have time to support
- redox as I don't use it in my job. If there is an active user who wants to be a full
- collaborator and take over improvements, please contact me!
```

Modern, asynchronous, and wicked fast C++11 client for Redis
[![Build Status](https://travis-ci.org/hmartiro/redox.svg?branch=feature%2Ftravis-ci)]
(https://travis-ci.org/hmartiro/redox)

Redox is a C++ interface to the
[Redis](http://redis.io/) key-value store that makes it easy to write applications
that are both elegant and high-performance. Communication should be a means to an
end, not something we spend a lot of time worrying about. Redox takes care of the
details so you can move on to the interesting part of your project.

**Features:**

 * Expressive asynchronous and synchronous API, templated by return value
 * Callbacks can be lambdas, class methods, bind expressions, or any
   [std::function](http://en.cppreference.com/w/cpp/utility/functional/function)
 * Thread-safe - use one client in multiple threads or multiple clients in one
 * Automatic pipelining, even for synchronous calls from separate threads
 * Low-level access when needed
 * Accessible and robust error handling
 * Configurable logging level and output to any ostream
 * Full support for binary data (keys and values)
 * Fast - developed for robotics applications
 * 100% clean Valgrind reports

Redox is built on top of
[hiredis](https://github.com/redis/hiredis/) and
[libev](http://manpages.ubuntu.com/manpages/raring/man3/ev.3.html). It uses only the
asynchronous API of hiredis, even for synchronous commands. There is no dependency on
Boost or any other libraries.

## Benchmarks
Benchmarks are given by averaging the results of ten trials of the speed tests
in `examples/` on an AWS t2.medium instance running Ubuntu 14.04 (64-bit) and a
local Redis server.

 * `speed_test_async_multi` over TCP: **879,589 commands/s**
 * `speed_test_async_multi` over Unix socket: **901,683 commands/s**
 * `speed_test_async` over TCP: **203,285 commands/s**
 * `speed_test_async` over Unix socket: **301,823 commands/s**
 * `speed_test_sync` over TCP: **21,072 commands/s**
 * `speed_test_sync` over Unix socket: **24,911 commands/s**

A mid-range laptop gives comparable results. Numbers can be much higher on a high-end machine.

## Tutorial
This section introduces the main features of redox. Look in `examples/` for more inspiration.

#### Hello world
Here is the simplest possible redox program:

```c++
#include <iostream>
#include <redox.hpp>

using namespace std;
using namespace redox;

int main(int argc, char* argv[]) {

  Redox rdx;
  if(!rdx.connect("localhost", 6379)) return 1;

  rdx.set("hello", "world!");
  cout << "Hello, " << rdx.get("hello") << endl;

  rdx.disconnect();
  return 0;
}
```

Compile and run:

    $ g++ hello.cpp -o hello -std=c++11 -lredox -lev -lhiredis
    $ ./hello
    Hello, world!

This example is synchronous, in the sense that the commands don't return until
a reply is received from the server.

#### Asynchronous commands
In a high-performance application, we don't want to wait for a reply, but instead
do other work. At the core of Redox is a generic asynchronous API for executing
any Redis command and providing a reply callback. The `command` method accepts a
Redis command in the form of an STL vector of strings, and a callback to be invoked
when a reply is received or if there is an error.

```c++
rdx.command<string>({"GET", "hello"}, [](Command<string>& c) {
  if(c.ok()) {
    cout << "Hello, async " << c.reply() << endl;
  } else {
    cerr << "Command has error code " << c.status() << endl;
  }
});
```

This statement tells redox to run the command `GET hello`. The `<string>` template
parameter means that we want the reply to be put into a string and that we expect
the server to respond with something that can be put into a string. The full list
of reply types is listed in this document and covers convenient access to anything
returned from the Redis protocol. The input vector can contain arbitrary binary
data.

The second argument is a callback function that accepts a reference to a Command object
of the requested reply type. The Command object contains the reply and any error
information. If `c.ok()` is true, the expected reply is accessed from
`c.reply()` (a string in this case). If `c.ok()` is false, then the error
code is given by `c.status()`, which can report an error or nil reply, a reply of
the wrong type, a send error, etc. The callback is guaranteed to be invoked
exactly once, and the memory for the Command object is freed automatically once
the callback returns.

Here is a simple example of running `GET hello` asynchronously ten times:

```c++
Redox rdx;

// Block until connected, localhost by default
if(!rdx.connect()) return 1;

auto got_reply = [](Command<string>& c) {
  if(!c.ok()) return;
  cout << c.cmd() << ": " << c.reply() << endl;
};

for(int i = 0; i < 10; i++) rdx.command<string>({"GET", "hello"}, got_reply);

// Do useful work
this_thread::sleep_for(chrono::milliseconds(10));

rdx.disconnect(); // Block until disconnected
```

The `.command()` method returns immediately, so this program doesn't wait for a reply
from the server - it just pauses for ten milliseconds and then shuts down. If we want to
shut down after we get all replies, we could do something like this:

```c++
Redox rdx;
if(!rdx.connect()) return 1;

int total = 10; // Number of commands to run
atomic_int count(0); // Number of replies expected
auto got_reply = [&](Command<string>& c) {
  count++;
  if(c.ok()) cout << c.cmd() << " #" << count << ": " << c.reply() << endl;
  if(count == total) rdx.stop(); // Signal to shut down
};

for(int i = 0; i < total; i++) rdx.command<string>({"GET", "hello"}, got_reply);

// Do useful work

rdx.wait(); // Block until shut down complete
```

This example tracks of how how many replies are received and signals the Redox
instance to stop once they all process. We use an `std::atomic_int` to be safe
because the callback is invoked from a separate thread. The `stop()` method
signals Redox to shut down its event loop and disconnect from Redis. The `wait()`
method blocks until `stop()` has been called and everything is brought down.
The `disconnect()` method used earlier is just a call to `stop()` and then a
call to `wait()`.

#### Synchronous commands
Redox implements synchronous commands by running asynchronous commands and waiting
on them with condition variables. That way, we can reap the benefits of pipelining
between synchronous commands in different threads. The `commandSync` method provides
a similar API to `command`, but instead of a callback returns a Command object when
a reply is received.

```c++
Command<string>& c = rdx.commandSync<string>({"GET", "hello"});
if(c.ok()) cout << c.cmd() << ": " << c.reply() << endl;
c.free();
```

When using synchronous commands, the user is responsible for freeing the memory of
the Command object by calling `c.free()`. The `c.cmd()` method just returns a string
representation of the command (`GET hello` in this case).

#### Looping and delayed commands
We often want to run commands on regular invervals. Redox provides the `commandLoop`
method to accomplish this. It is easier to use and more efficient than running individual
commands in a loop, because it only creates a single Command object.
`commandLoop` takes a command vector, a callback, and an interval (in seconds)
to repeat the command. It then runs the command on the given interval until the user
calls `c.free()`.

```c++
Command<string>& cmd = rdx.commandLoop<string>({"GET", "hello"}, [](Command<string>& c) {
  if(c.ok()) cout << c.cmd() << ": " << c.reply() << endl;
}, 0.1);

this_thread::sleep_for(chrono::seconds(1));
cmd.free();
rdx.disconnect();
```

Finally, `commandDelayed` runs a command after a specified delay (in seconds). It does
not return a command object, because the memory is automatically freed after the callback
is invoked.

```c++
rdx.commandDelayed<string>({"GET", "hello"}, [](Command<string>& c) {
  if(c.ok()) cout << c.cmd() << ": " << c.reply() << endl;
}, 1);
this_thread::sleep_for(chrono::seconds(2));
```

#### Convenience methods
The four methods `command`, `commandSync`, `commandLoop`, and `commandDelayed` form
the core of Redox's functionality. There are convenience methods provided that are
simple wrappers over the core methods. Some examples of those are `.get()`, `.set()`,
`.del()`, and `.publish()`. These methods are nice because they return simple values,
and there are no Command objects or template parameters. However, they make strong
assumptions about how to deal with errors (ignore or throw exceptions), and since
their implementations are a few lines of code it is often easier to create custom
convenience methods for your application.

#### Publisher / Subscriber
Redox provides an API for the pub/sub functionality of Redis. Publishing is done just like
any other command using a Redox instance. There is a separate Subscriber class that
receives messages and provides subscribe/unsubscribe and psubscribe/punsubscribe methods.

```c++
Redox rdx; Subscriber sub;
if(!rdx.connect() || !sub.connect()) return 1;

sub.subscribe("hello", [](const string& topic, const string& msg) {
  cout << topic << ": " << msg << endl;
});

for(int i = 0; i < 10; i++) {
  rdx.publish("hello", "this is a pubsub message");
  this_thread::sleep_for(chrono::milliseconds(500));
}

sub.disconnect(); rdx.disconnect();
```

#### strToVec and vecToStr
Redox provides helper methods to convert between a string command and
a vector of strings as needed by its API. `rdx.strToVec("GET foo")`
will return an `std::vector<std::string>` containing `GET` and `foo`
as entries. `rdx.vecToStr({"GET", "foo"})` will return the string `GET foo`.

#### No-Wait Mode
Redox provides a no-wait mode, which tells the event loop not to sleep
in between processing events. It means that the event thread will run
at 100% CPU, but it can greatly improve performance when critical. It is
disabled by default and can be enabled with `rdx.noWait(true);`.

## Reply types
These the available template parameters in redox and the Redis
[return types](http://redis.io/topics/protocol) they can hold.
If a given command returns an incompatible type you will get
a `WRONG_TYPE` or `NIL_REPLY` status.

 * `<redisReply*>`: All reply types, returns the hiredis struct directly
 * `<char*>`: Simple Strings, Bulk Strings
 * `<std::string>`: Simple Strings, Bulk Strings
 * `<long long int>`: Integers
 * `<int>`: Integers (careful about overflow, `long long int` recommended)
 * `<std::nullptr_t>`: Null Bulk Strings, any other receiving a nil reply will get a NIL_REPLY status
 * `<std::vector<std::string>>`: Arrays of Simple Strings or Bulk Strings (in received order)
 * `<std::set<std::string>>`: Arrays of Simple Strings or Bulk Strings (in sorted order)
 * `<std::unordered_set<std::string>>`: Arrays of Simple Strings or Bulk Strings (in no order)

## Installation
Instructions provided are for Ubuntu, but all components are platform-independent.

#### Build from source
Get the build environment and dependencies:

    sudo apt-get install git cmake build-essential
    sudo apt-get install libhiredis-dev libev-dev

Build the library:

    mkdir build && cd build
    cmake ..
    make

Install into system directories (optional):

    sudo make install

#### Build examples and test suite
Enable examples using ccmake or the following:

    cmake -Dexamples=ON ..
    make examples

To run the test suite, first make sure you have
[gtest](https://code.google.com/p/googletest/) set up,
then:

    cmake -Dtests=ON ..
    make test_redox
    ./test_redox

#### Build documentation
Redox documentation is generated using [doxygen](http://doxygen.org).

    cd docs
    doxygen

The documentation can then be viewed in a browser at `docs/html/index.html`.

#### Build RPM and DEB packages
Basic support to build RPMs and DEBs is in the build system. To build them, issue
the following commands:

    mkdir release && cd release
    cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
    make package

NOTE: To build RPM packages, you will need rpmbuild.

## Contributing
Redox is in its early stages and I am looking for feedback and contributors to make
it easier, faster, and more robust. Open issues on GitHub or message me directly.

Redox is not currently recommended for production use. It has no support yet for sentinels
or clusters. Feel free to provide them!
