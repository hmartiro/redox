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

#include <algorithm>
#include <iostream>
#include <set>
#include <list>

#include <gtest/gtest.h>

#include "redox.hpp"

namespace {

using namespace std;
using redox::Redox;
using redox::Command;

static const std::string TEST_REDIS_HOST="localhost";
static const int TEST_REDIS_PORT = 6380;


// ------------------------------------------
// The fixture for testing class Redox.
// ------------------------------------------
class RedoxTest : public ::testing::Test {

protected:
  Redox rdx;

  RedoxTest() {}

  void connect() {

    // Connect to the server
    rdx.connect(TEST_REDIS_HOST, TEST_REDIS_PORT);

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

TEST_F(RedoxTest, TestConnection) {
  EXPECT_TRUE(rdx.connect(TEST_REDIS_HOST, TEST_REDIS_PORT)); }

TEST_F(RedoxTest, TestConnectionFailure) {
  EXPECT_FALSE(rdx.connect(TEST_REDIS_HOST, TEST_REDIS_PORT + 1000)); }

TEST_F(RedoxTest, GetSet) {
  connect();
  rdx.command<string>({"SET", "redox_test:a", "apple"}, print_and_check<string>("OK"));
  rdx.command<string>({"GET", "redox_test:a"}, print_and_check<string>("apple"));
  wait_for_replies();

}

TEST_F(RedoxTest, Exists)
{
  std::string key = "redox_test:x";
  connect();
  ASSERT_FALSE(rdx.exists(key));
  ASSERT_TRUE(rdx.set(key, "1"));
  ASSERT_TRUE(rdx.exists(key));
  ASSERT_TRUE(rdx.del(key));
  ASSERT_FALSE(rdx.exists(key));
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

  try {
    rdx.commandSync<int>({"DEL", "redox_test:mt"});
  }
  catch (...) {
    // make sure we clean the instance
  }
}

// -------------------------------------------
// Test SET interface - synchronous
// -------------------------------------------
TEST_F(RedoxTest, SetSync) {
  connect();
  std::string set_key = "redox_test:set";
  std::vector<std::string> members = {"200", "300", "400"};
  ASSERT_EQ(members.size(), rdx.sadd(set_key, members));
  ASSERT_TRUE(rdx.sadd(set_key, "100"));
  ASSERT_FALSE(rdx.sadd(set_key, "400"));
  (void) members.push_back("100");
  ASSERT_TRUE(rdx.sismember(set_key, "300"));
  ASSERT_FALSE(rdx.sismember(set_key, "1500"));
  ASSERT_EQ(4, rdx.scard(set_key));
  auto ret_members = rdx.smembers(set_key);

  for (const auto& elem: members)
    ASSERT_TRUE(ret_members.find(elem) != ret_members.end());

  ASSERT_TRUE(rdx.srem(set_key, "100"));
  ASSERT_EQ(3, rdx.srem(set_key, members));
  ASSERT_FALSE(rdx.srem(set_key, "100"));
  ASSERT_EQ(0, rdx.smembers("fake_key").size());
  ASSERT_EQ(0, rdx.scard(set_key));

  // Test SSCAN functionality
  members.clear();

  for (int i = 0; i < 3000; ++i)
  {
    members.push_back(std::to_string(i));
    ASSERT_TRUE(rdx.sadd(set_key, i));
  }

  long long cursor = 0;
  long long count = 1000;
  std::pair< long long, std::vector<std::string> > reply;

  do
  {
    reply = rdx.sscan(set_key, cursor, count);
    cursor = reply.first;

    for (auto&& elem: reply.second)
    {
      ASSERT_TRUE(std::find(members.begin(), members.end(), elem) !=
		  members.end());
    }
  }
  while (cursor);

  print_and_check_sync(rdx.commandSync<int>({"DEL", set_key}), 1);
  rdx.disconnect();
}

// -------------------------------------------
// Test SET interface - asynchronous
// -------------------------------------------
TEST_F(RedoxTest, SetAsync) {
  connect();
  std::string set_key = "redox_test:set_async";
  std::vector<std::string> members = {"200", "300", "400"};
  std::list<std::string> lst_errors;
  std::atomic<std::uint64_t> num_asyn_req {0};
  std::mutex mutex;
  std::condition_variable cond_var;
  auto callback = [&](Command<int>& c) {
    if ((c.ok() && c.reply() != 1) || !c.ok())
    {
      std::ostringstream oss;
      oss << "Failed command: " << c.cmd() << " error: " << c.lastError();
      lst_errors.emplace(lst_errors.end(), oss.str());
    }

    if (--num_asyn_req == 0)
      cond_var.notify_one();
  };

  std::string value;
  // Add some elements
  for (auto i = 0; i < 100; ++i)
  {
    value = "val" + std::to_string(i);
    num_asyn_req++;
    rdx.sadd(set_key, value, callback);
  }

  // Wait for all the replies
  {
    std::unique_lock<std::mutex> lock(mutex);
    while (num_asyn_req)
      cond_var.wait(lock);
  }

  ASSERT_EQ(0, lst_errors.size());
  // Add some more elements that will trigger some errors
  for (auto i = 90; i < 110; ++i)
  {
    value = "val" + std::to_string(i);
    num_asyn_req++;
    rdx.sadd(set_key, value, callback);
  }

  // Wait for all the replies
  {
    std::unique_lock<std::mutex> lock(mutex);
    while (num_asyn_req)
      cond_var.wait(lock);
  }

  ASSERT_EQ(10, lst_errors.size());
  ASSERT_EQ(110, rdx.scard(set_key));
  lst_errors.clear();

  // Remove all elements
  for (auto i = 0; i < 110; ++i)
  {
    num_asyn_req++;
    value = "val" + std::to_string(i);
    rdx.srem(set_key, value, callback);
  }

  // Wait for all the replies
  {
    std::unique_lock<std::mutex> lock(mutex);
    while (num_asyn_req)
      cond_var.wait(lock);
  }

  ASSERT_EQ(0, lst_errors.size());
  print_and_check_sync(rdx.commandSync<int>({"DEL", set_key}), 0);
  rdx.disconnect();
}

// -------------------------------------------
// Test HASH interface - synchronous
// -------------------------------------------
TEST_F(RedoxTest, HashSync) {
  connect();
  std::string hash_key = "redox_test:hash";
  std::vector<std::string> fields {"val1", "val2", "val3"};
  std::vector<int> ivalues {10, 20, 30};
  std::vector<float> fvalues {100.0, 200.0, 300.0};
  std::vector<std::string> svalues {"1000", "2000", "3000"};
  ASSERT_EQ(0, rdx.hlen(hash_key));
  ASSERT_TRUE(rdx.hset(hash_key, fields[0], fvalues[0]));
  ASSERT_FLOAT_EQ(fvalues[0], std::stof(rdx.hget(hash_key, fields[0])));
  ASSERT_FLOAT_EQ(100.0005, rdx.hincrbyfloat(hash_key, fields[0], 0.0005));
  ASSERT_TRUE(rdx.hexists(hash_key, fields[0]));
  ASSERT_TRUE(rdx.hdel(hash_key, fields[0]));

  ASSERT_FALSE(rdx.hexists(hash_key, fields[1]));
  ASSERT_TRUE(rdx.hsetnx(hash_key, fields[1], svalues[1]));
  ASSERT_FALSE(rdx.hsetnx(hash_key, fields[1], svalues[1]));
  ASSERT_EQ(svalues[1], rdx.hget(hash_key, fields[1]));
  ASSERT_TRUE(rdx.hdel(hash_key, fields[1]));

  ASSERT_TRUE(rdx.hset(hash_key, fields[2], ivalues[2]));
  ASSERT_TRUE(rdx.hset(hash_key, fields[1], ivalues[1]));
  ASSERT_EQ(35, rdx.hincrby(hash_key, fields[2], 5));
  ASSERT_TRUE(rdx.hdel(hash_key, fields[2]));
  ASSERT_TRUE(rdx.hsetnx(hash_key, fields[2], ivalues[2]));
  ASSERT_TRUE(rdx.hsetnx(hash_key, fields[0], ivalues[0]));
  ASSERT_EQ(3, rdx.hlen(hash_key));

  // Test the hkeys command
  std::vector<std::string> resp = rdx.hkeys(hash_key);

  for (auto&& elem: resp)
  {
    ASSERT_TRUE(std::find(fields.begin(), fields.end(), elem) != fields.end());
  }

  // Test the hvals command
  resp = rdx.hvals(hash_key);

  for (auto&& elem: resp)
  {
    ASSERT_TRUE(std::find(ivalues.begin(), ivalues.end(), std::stoi(elem)) != ivalues.end());
  }

  // Test the hgetall command
  resp = rdx.hgetall(hash_key);

  for (auto it = resp.begin(); it != resp.end(); ++it)
  {
    ASSERT_TRUE(std::find(fields.begin(), fields.end(), *it) != fields.end());
    ++it;
    ASSERT_TRUE(std::find(ivalues.begin(), ivalues.end(), std::stoi(*it)) != ivalues.end());
  }

  ASSERT_TRUE(rdx.hget(hash_key, "dummy_field").empty());
  ASSERT_TRUE(rdx.hget("unknown_key", "dummy_field").empty());
  print_and_check_sync(rdx.commandSync<int>({"DEL", hash_key}), 1);

  // Test hscan command
  std::unordered_map<int, int> map;
  std::unordered_map<int, int> ret_map;
  for (int i = 0; i < 3000; ++i)
  {
    map.emplace(i, i);
    ASSERT_TRUE(rdx.hset(hash_key, std::to_string(i), i));
  }

  long long cursor = 0;
  long long count = 1000;
  std::pair< long long , std::unordered_map<std::string, std::string> > reply;
  reply = rdx.hscan(hash_key, cursor, count);
  cursor = reply.first;

  for (auto&& elem: reply.second)
  {
    ASSERT_TRUE(map[std::stoi(elem.first)] == std::stoi(elem.second));
    ret_map.emplace(std::stoi(elem.first), std::stoi(elem.second));
  }

  while (cursor)
  {
    reply = rdx.hscan(hash_key, cursor, count);
    cursor = reply.first;

    for (auto&& elem: reply.second)
    {
      ASSERT_TRUE(map[std::stoi(elem.first)] == std::stoi(elem.second));
      ret_map.emplace(std::stoi(elem.first), std::stoi(elem.second));
    }
  }

  ASSERT_TRUE(map.size() == ret_map.size());
  print_and_check_sync(rdx.commandSync<int>({"DEL", hash_key}), 1);
  rdx.disconnect();
}

// -------------------------------------------
// Test HASH interface - asynchronous
// -------------------------------------------
TEST_F(RedoxTest, HashAsync) {
  connect();
  std::string hash_key = "redox_test:hash_async";
  ASSERT_EQ(0, rdx.hlen(hash_key));
  std::string field, value;
  std::list<std::string> lst_errors;
  std::atomic<std::uint64_t> num_async_req {0};
  std::condition_variable wait_cv;
  std::mutex mutex;
  std::uint64_t num_elem = 100;
  auto callback_set = [&](Command<int>& c) {
    if (!c.ok())
    {
      if (c.cmd_.size() >= 3)
      {
	std::string cmd_field = c.cmd_[2];
	lst_errors.emplace(lst_errors.end(), cmd_field);
      }
    }

    if (!--num_async_req)
      wait_cv.notify_one();

    c.free();
  };

  // Push asynchronously num_elem
  for (std::uint64_t i = 0; i < num_elem; ++i)
  {
    num_async_req++;
    field = "field" + std::to_string(i);
    value = std::to_string(i);
    rdx.hset(hash_key, field, value, callback_set);
  }

  {
    // Wait for all the async requests
    std::unique_lock<std::mutex> lock(mutex);
    while (num_async_req)
      wait_cv.wait(lock);
  }

  ASSERT_EQ(0, lst_errors.size());

  // Get map length asynchronously
  long long int length = 0;
  auto callback_len = [&](Command<long long int>& c) {
    if (!c.ok())
      length = -1;
    else
      length = c.reply();

    c.free();
    wait_cv.notify_one();
  };

  rdx.hlen(hash_key, callback_len);

  {
    // Wait for length async response
    std::unique_lock<std::mutex> lock(mutex);
    wait_cv.wait(lock);
  }

  ASSERT_EQ(num_elem, length);

  // Delete asynchronously all elements
  auto callback_del = [&](Command<int>& c) {
    if (!c.ok())
    {
      if (c.cmd_.size() >= 3)
      {
	std::string cmd_field = c.cmd_[2];
	lst_errors.emplace(lst_errors.end(), cmd_field);
      }
    }

    if (!--num_async_req)
      wait_cv.notify_one();
  };

  for (std::uint64_t i = 0; i <= num_elem; ++i)
  {
    num_async_req++;
    field = "field" + std::to_string(i);
    rdx.hdel(hash_key, field, callback_del);
  }

  {
    // Wait for all the async requests
    std::unique_lock<std::mutex> lock(mutex);
    while (num_async_req)
      wait_cv.wait(lock);
  }

  ASSERT_EQ(0, lst_errors.size());
  ASSERT_EQ(0, rdx.hlen(hash_key));
  rdx.disconnect();
}

// -------------------------------------------
// End tests
// -------------------------------------------

} // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
