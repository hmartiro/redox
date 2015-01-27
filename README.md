redox
======

Modern, asynchronous, and wicked fast C++11 client for Redis

Redox is a C++ interface to the
[Redis](http://redis.io/) key-value store that makes it easy to write applications
that are both elegant and high-performance. Communication should be a means to an
end, not something we spend a lot of time worrying about. Redox takes care of the
details so you can move on to the interesting part.

**Features:**

 * Runs Redis commands from strings, no explicit wrappers for the API
 * Callbacks can be lambdas, class methods, bind expressions - anything
   [std::function](http://en.cppreference.com/w/cpp/utility/functional/function) can hold
 * Fully thread-safe - use one Redox object in multiple threads or multiple Redox objects in one thread
 * Automatic pipelining, even for synchronous calls from separate threads
 * Access to low-level reply objects when needed
 * 100% clean Valgrind reports
 * Accessible and robust error handling
 * Fast - developed for robotics applications

Redox is built on top of
[hiredis](https://github.com/redis/hiredis/) and
[libev](http://manpages.ubuntu.com/manpages/raring/man3/ev.3.html). It uses only the
asynchronous API of hiredis, even for synchronous commands.

## Performance Benchmarks
Benchmarks are given by averaging the results of five trials of the speed tests
in `examples/` on an AWS t2.medium instance running Ubuntu 14.04 (64-bit).

Local Redis server, TCP connection:

 * 100 commandLoop calls (`speed_test_async_multi`): **710,014 commands/s**
 * One commandLoop call (`speed_test_async`): **195,159 commands/s**
 * Looped commandSync call  (`speed_test_sync`): **23,609 commands/s**

Results are comparable to that of an
average laptop. On a high-end laptop or PC, `speed_test_async_multi` usually tops
1 million commands per second. All results are slightly faster if over Unix sockets
than TCP.

## Build library from source
Instructions provided are for Ubuntu, but Redox is fully platform-independent.

Get the build environment:

    sudo apt-get install git cmake build-essential

Get the dependencies:

    sudo apt-get install libhiredis-dev libev-dev

Build the library:

    mkdir build && cd build
    cmake ..
    make

Install into the system (optional):

    sudo make install

## Build examples and test suite
To build the examples, use ccmake or the following:

    cmake -Dexamples=ON ..
    make examples

To run the test suite, first make sure you have
[gtest](https://code.google.com/p/googletest/) set up,
then:

    cmake -Dtests=ON ..
    make test_redox
    ./test_redox

## Tutorial
Coming soon. Take a look at `examples/` for now.
