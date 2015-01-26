/**
* Test suite for Redox using GTest.
*/

#include <iostream>

#include "../src/redox.hpp"
#include "gtest/gtest.h"

namespace {

using namespace std;
using redox::Redox;
using redox::Command;

// ------------------------------------------
// The fixture for testing class Redox.
// ------------------------------------------
class RedoxTest : public ::testing::Test {

protected:

  Redox rdx = {"localhost", 6379};

  RedoxTest() {

    // Connect to the server
    rdx.start();

    // Clear all keys used by the tests here
    rdx.command("DEL redox_test:a");
  }

  virtual ~RedoxTest() {

    // Block until the event loop exits.
    // Each test is responsible for calling the stop_signal()
    rdx.block();
  }

  // CV and counter to wait for async commands to complete
  atomic_int cmd_count = {0};
  condition_variable cmd_waiter;
  mutex cmd_waiter_lock;

  // To make the callback code nicer
  template<class ReplyT>
  using Callback = std::function<void(Command<ReplyT>&)>;

  /**
  * Helper function that returns a command callback to print out the
  * command/reply and to test the reply against the provided value.
  */
  template<class ReplyT>
  Callback<ReplyT> check(const ReplyT& value) {
    cmd_count++;
    return [this, value](Command<ReplyT>& c) {
      EXPECT_TRUE(c.ok());
      if(c.ok()) EXPECT_EQ(c.reply(), value);
      cmd_count--;
      cmd_waiter.notify_all();
    };
  }

  /**
  * Wrapper for the callback that also prints out the command.
  */
  template<class ReplyT>
  Callback<ReplyT> print(Callback<ReplyT> callback) {
    return [callback](Command<ReplyT>& c) {
      if(c.ok()) cout << "[ASYNC] " << c.cmd() << ": " << c.reply() << endl;
      callback(c);
    };
  }

  /**
  * Combination of print and check for simplicity.
  */
  template<class ReplyT>
  Callback<ReplyT> print_and_check(const ReplyT& value) {
    return print<ReplyT>(check<ReplyT>(value));
  }

  /**
  * Wait until all async commands that used check() as a callback
  * complete.
  */
  void wait_and_stop() {
    unique_lock<mutex> ul(cmd_waiter_lock);
    cmd_waiter.wait(ul, [this] { return (cmd_count == 0); });
    rdx.stop_signal();
  };

  template<class ReplyT>
  void check_sync(Command<ReplyT>& c, const ReplyT& value) {
    ASSERT_TRUE(c.ok());
    EXPECT_EQ(c.reply(), value);
    c.free();
  }

  template<class ReplyT>
  void print_and_check_sync(Command<ReplyT>& c, const ReplyT& value) {
    ASSERT_TRUE(c.ok());
    EXPECT_EQ(c.reply(), value);
    cout << "[SYNC] " << c.cmd_ << ": " << c.reply() << endl;
    c.free();
  }
};

// -------------------------------------------
// Core unit tests - asynchronous
// -------------------------------------------

TEST_F(RedoxTest, GetSet) {
  rdx.command<string>("SET redox_test:a apple", print_and_check<string>("OK"));
  rdx.command<string>("GET redox_test:a",  print_and_check<string>("apple"));
  wait_and_stop();
}

TEST_F(RedoxTest, Delete) {
  rdx.command<string>("SET redox_test:a apple", print_and_check<string>("OK"));
  rdx.command<int>("DEL redox_test:a", print_and_check(1));
  rdx.command<nullptr_t>("GET redox_test:a", check(nullptr));
  wait_and_stop();
}

TEST_F(RedoxTest, Incr) {
  int count = 100;
  for(int i = 0; i < count; i++) {
    rdx.command<int>("INCR redox_test:a", check(i+1));
  }
  rdx.command<string>("GET redox_test:a", print_and_check(to_string(count)));
  wait_and_stop();
}

TEST_F(RedoxTest, Delayed) {
  Command<int>& c = rdx.command_looping<int>("INCR redox_test:a", check(1), 0, 0.1);
  this_thread::sleep_for(chrono::milliseconds(150));
  c.cancel();
  rdx.command<string>("GET redox_test:a", print_and_check(to_string(1)));
  wait_and_stop();
}

TEST_F(RedoxTest, Loop) {
  int count = 0;
  int target_count = 100;
  double dt = 0.001;
  Command<int>& cmd = rdx.command_looping<int>("INCR redox_test:a",
      [this, &count](Command<int>& c) {
        check(++count)(c);
      },
      dt
  );

  double wait_time = dt * (target_count - 0.5);
  this_thread::sleep_for(std::chrono::duration<double>(wait_time));
  cmd.cancel();

  rdx.command<string>("GET redox_test:a", print_and_check(to_string(target_count)));
  wait_and_stop();
}

// -------------------------------------------
// Core unit tests - synchronous
// -------------------------------------------

TEST_F(RedoxTest, GetSetSync) {
  print_and_check_sync<string>(rdx.command_blocking<string>("SET redox_test:a apple"), "OK");
  print_and_check_sync<string>(rdx.command_blocking<string>("GET redox_test:a"), "apple");
  rdx.stop_signal();
}

TEST_F(RedoxTest, DeleteSync) {
  print_and_check_sync<string>(rdx.command_blocking<string>("SET redox_test:a apple"), "OK");
  print_and_check_sync(rdx.command_blocking<int>("DEL redox_test:a"), 1);
  check_sync(rdx.command_blocking<nullptr_t>("GET redox_test:a"), nullptr);
  rdx.stop_signal();
}

TEST_F(RedoxTest, IncrSync) {
  int count = 100;
  for(int i = 0; i < count; i++) {
    check_sync(rdx.command_blocking<int>("INCR redox_test:a"), i+1);
  }
  print_and_check_sync(rdx.command_blocking<string>("GET redox_test:a"), to_string(count));
  rdx.stop_signal();
}

// -------------------------------------------
// End tests
// -------------------------------------------

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
