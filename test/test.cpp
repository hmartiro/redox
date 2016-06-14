/*
* Redox - A modern, asynchronous, and wicked fast C++11 client for Redis
*
*    https://github.com/hmartiro/redox
*
* Copyright 2015 - Hayk Martirosyan <hayk.mart@gmail.com>
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

#include <iostream>

#include <gtest/gtest.h>

#include "redox.hpp"

namespace {

using namespace std;
using redox::Redox;
using redox::Command;

// ------------------------------------------
// The fixture for testing class Redox.
// ------------------------------------------
class RedoxTest : public ::testing::Test {

protected:
  Redox rdx;

  RedoxTest() {}

  void connect() {

    // Connect to the server
    rdx.connect("localhost", 6379);

    // Clear all keys used by the tests here
    rdx.command({"DEL", "redox_test:a"});
  }

  virtual ~RedoxTest() {}

  // CV and counter to wait for async commands to complete
  atomic_int cmd_count = {0};
  condition_variable cmd_waiter;
  mutex cmd_waiter_lock;

  // To make the callback code nicer
  template <class ReplyT> using Callback = std::function<void(Command<ReplyT> &)>;

  /**
  * Helper function that returns a command callback to print out the
  * command/reply and to test the reply against the provided value.
  */
  template <class ReplyT> Callback<ReplyT> check(const ReplyT &value) {
    cmd_count++;
    return [this, value](Command<ReplyT> &c) {
      EXPECT_TRUE(c.ok());
      if (c.ok())
        EXPECT_EQ(value, c.reply());
      cmd_count--;
      cmd_waiter.notify_all();
    };
  }

  /**
  * Wrapper for the callback that also prints out the command.
  */
  template <class ReplyT> Callback<ReplyT> print(Callback<ReplyT> callback) {
    return [callback](Command<ReplyT> &c) {
      if (c.ok())
        cout << "[ASYNC] " << c.cmd() << ": " << c.reply() << endl;
      callback(c);
    };
  }

  /**
  * Combination of print and check for simplicity.
  */
  template <class ReplyT> Callback<ReplyT> print_and_check(const ReplyT &value) {
    return print<ReplyT>(check<ReplyT>(value));
  }

  /**
  * Check the error
  */
  template <class ReplyT> Callback<ReplyT> print_and_check_error(const ReplyT &value) {
    cmd_count++;
    return [this, value](Command<ReplyT> &c) {
      EXPECT_FALSE(c.ok());
      EXPECT_FALSE(c.lastError().empty());
      //      EXPECT_EQ(value, c.reply());
      cout << c.cmd() << ": " << c.lastError() << endl;
      cmd_count--;
      cmd_waiter.notify_all();
    };
  }

  /**
  * Wait until all async commands that used check() as a callback
  * complete.
  */
  void wait_for_replies() {
    unique_lock<mutex> ul(cmd_waiter_lock);
    cmd_waiter.wait(ul, [this] { return (cmd_count == 0); });
    rdx.disconnect();
  }

  template <class ReplyT> void check_sync(Command<ReplyT> &c, const ReplyT &value) {
    ASSERT_TRUE(c.ok());
    EXPECT_EQ(c.reply(), value);
    c.free();
  }

  template <class ReplyT> void print_and_check_sync(Command<ReplyT> &c, const ReplyT &value) {
    ASSERT_TRUE(c.ok());
    EXPECT_EQ(c.reply(), value);
    cout << "[SYNC] " << c.cmd() << ": " << c.reply() << endl;
    c.free();
  }

  /**
  * Check the error
  */
  template <class ReplyT> void print_and_check_error_sync(Command<ReplyT> &c, const ReplyT &value) {
    EXPECT_FALSE(c.ok());
    EXPECT_FALSE(c.lastError().empty());
    //      EXPECT_EQ(value, c.reply());
    cout << c.cmd() << ": " << c.lastError() << endl;
  }
};

// -------------------------------------------
// Core unit tests - asynchronous
// -------------------------------------------

TEST_F(RedoxTest, TestConnection) { EXPECT_TRUE(rdx.connect("localhost", 6379)); }

TEST_F(RedoxTest, TestConnectionFailure) { EXPECT_FALSE(rdx.connect("localhost", 6380)); }

TEST_F(RedoxTest, GetSet) {
  connect();
  rdx.command<string>({"SET", "redox_test:a", "apple"}, print_and_check<string>("OK"));
  rdx.command<string>({"GET", "redox_test:a"}, print_and_check<string>("apple"));
  wait_for_replies();
}

TEST_F(RedoxTest, Delete) {
  connect();
  rdx.command<string>({"SET", "redox_test:a", "apple"}, print_and_check<string>("OK"));
  rdx.command<int>({"DEL", "redox_test:a"}, print_and_check(1));
  rdx.command<nullptr_t>({"GET", "redox_test:a"}, check(nullptr));
  wait_for_replies();
}

TEST_F(RedoxTest, Incr) {
  connect();
  int count = 100;
  for (int i = 0; i < count; i++) {
    rdx.command<int>({"INCR", "redox_test:a"}, check(i + 1));
  }
  rdx.command<string>({"GET", "redox_test:a"}, print_and_check(to_string(count)));
  wait_for_replies();
}

TEST_F(RedoxTest, Delayed) {
  connect();
  rdx.commandDelayed<int>({"INCR", "redox_test:a"}, check(1), 0.1);
  this_thread::sleep_for(chrono::milliseconds(150));
  rdx.command<string>({"GET", "redox_test:a"}, print_and_check(to_string(1)));
  wait_for_replies();
}

TEST_F(RedoxTest, Loop) {
  connect();
  int count = 0;
  int target_count = 20;
  double dt = 0.005;
  Command<int> &cmd = rdx.commandLoop<int>(
      {"INCR", "redox_test:a"}, [this, &count](Command<int> &c) { check(++count)(c); }, dt);

  double wait_time = dt * (target_count - 0.5);
  this_thread::sleep_for(std::chrono::duration<double>(wait_time));
  cmd.free();

  rdx.command<string>({"GET", "redox_test:a"}, print_and_check(to_string(target_count)));
  wait_for_replies();
}

TEST_F(RedoxTest, GetSetError) {
  connect();
  rdx.command<string>({"SET", "redox_test:a", "apple"}, print_and_check<string>("OK"));
  rdx.command<int>({"GET", "redox_test:a"}, print_and_check_error<int>(3));
  wait_for_replies();
}

// -------------------------------------------
// Core unit tests - synchronous
// -------------------------------------------

TEST_F(RedoxTest, GetSetSync) {
  connect();
  print_and_check_sync<string>(rdx.commandSync<string>({"SET", "redox_test:a", "apple"}), "OK");
  print_and_check_sync<string>(rdx.commandSync<string>({"GET", "redox_test:a"}), "apple");
  rdx.disconnect();
}

TEST_F(RedoxTest, DeleteSync) {
  connect();
  print_and_check_sync<string>(rdx.commandSync<string>({"SET", "redox_test:a", "apple"}), "OK");
  print_and_check_sync(rdx.commandSync<int>({"DEL", "redox_test:a"}), 1);
  check_sync(rdx.commandSync<nullptr_t>({"GET", "redox_test:a"}), nullptr);
  rdx.disconnect();
}

TEST_F(RedoxTest, IncrSync) {
  connect();
  int count = 100;
  for (int i = 0; i < count; i++) {
    check_sync(rdx.commandSync<int>({"INCR", "redox_test:a"}), i + 1);
  }
  print_and_check_sync(rdx.commandSync<string>({"GET", "redox_test:a"}), to_string(count));
  rdx.disconnect();
}

TEST_F(RedoxTest, GetSetSyncError) {
  connect();
  print_and_check_sync<string>(rdx.commandSync<string>({"SET", "redox_test:a", "apple"}), "OK");
  print_and_check_error_sync<int>(rdx.commandSync<int>({"GET", "redox_test:a"}), 3);
  rdx.disconnect();
}

TEST_F(RedoxTest, MultithreadedCRUD) {
  connect();
  int create_count(0);
  int delete_count(0);
  int createExcCount(0);
  int deleteExcCount(0);

  std::mutex startMutex;
  bool start = false;
  std::condition_variable start_cv;
  const int count = 10000;

  std::thread create_thread([&]() {
    {
      std::unique_lock<std::mutex> lock(startMutex);
      start_cv.wait(lock, [&]() { return start; });
    }
    for (int i = 0; i < count; ++i) {
      try {
        rdx.commandSync<string>({"SET", "redox_test:mt", "create"});
      }
      catch (...) {
        createExcCount++;
      }
      create_count++;
    }
  });

  std::thread delete_thread([&]() {
    {
      std::unique_lock<std::mutex> lock(startMutex);
      start_cv.wait(lock, [&]() { return start; });
    }
    for (int i = 0; i < count; ++i) {
      try {
        rdx.commandSync<int>({"DEL", "redox_test:mt"});
      }
      catch (...) {
        deleteExcCount++;
      }
      delete_count++;
    }
  });

  // Start threads
  {
    std::lock_guard<std::mutex> lock(startMutex);
    start = true;
  }
  start_cv.notify_all();

  // Wait for threads to finish
  create_thread.join();
  delete_thread.join();
  EXPECT_EQ(count, create_count);
  EXPECT_EQ(count, delete_count);
}

// -------------------------------------------
// End tests
// -------------------------------------------

} // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
