/**
* Test suite for Redox using GTest.
*/

#include <iostream>

#include "../src/redox.hpp"
#include "gtest/gtest.h"

namespace {

using namespace redox;
using namespace std;

// ------------------------------------------
// The fixture for testing class Redox.
// ------------------------------------------
class RedoxTest : public ::testing::Test {
protected:

  Redox rdx = {"localhost", 6379};

  RedoxTest() {
    rdx.start();
  }

  virtual ~RedoxTest() {
    rdx.block();
  }

  // CV and counter to wait for async commands to complete
  atomic_int cmd_count = {0};
  condition_variable cmd_waiter;
  mutex cmd_waiter_lock;

  // To make the callback code nicer
  template <class ReplyT>
  using Callback = std::function<void(const std::string&, const ReplyT&)>;

  /**
  * Helper function that returns a command callback to print out the
  * command/reply and to test the reply against the provided value.
  */
  template<class ReplyT>
  Callback<ReplyT> check(const ReplyT& value) {
    cmd_count++;
    return [this, value](const string& cmd, const ReplyT& reply) {
      EXPECT_EQ(reply, value);
      cmd_count--;
      cmd_waiter.notify_all();
    };
  }

  /**
  * Wrapper for the callback that also prints out the command.
  */
  template<class ReplyT>
  Callback<ReplyT> print(Callback<ReplyT> callback) {
    return [callback](const string& cmd, const ReplyT& reply) {
      cout << cmd << ": " << reply << endl;
      callback(cmd, reply);
    };
  }

  /**
  * Combination of print and check for simplicity.
  */
  template<class ReplyT>
  Callback<ReplyT> print_and_check(const ReplyT& value) {
    return print(check<ReplyT>(value));
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
};

// -------------------------------------------
// Unit tests - asynchronous
// -------------------------------------------

TEST_F(RedoxTest, GetSet) {
  rdx.command<string>("SET redox_test:a apple", print_and_check<string>("OK"));
  rdx.command<string>("GET redox_test:a",  print_and_check<string>("apple"));
  wait_and_stop();
}

TEST_F(RedoxTest, Delete) {
  rdx.command<string>("SET redox_test:b banana", print_and_check<string>("OK"));
  rdx.command<int>("DEL redox_test:b", print_and_check(1));
  rdx.command<nullptr_t>("GET redox_test:b", check(nullptr));
  wait_and_stop();
}

// -------------------------------------------
// End tests
// -------------------------------------------

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
