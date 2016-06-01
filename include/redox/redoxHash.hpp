/*
 * Redox - A modern, asynchronous, and wicked fast C++11 client for Redis
 *
 *    https://github.com/hmartiro/redox
 *
 * Copyright 2016 - Elvin Sindrilaru <esindril at cern dot ch>
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

#pragma once
#include <vector>
#include <set>
#include "client.hpp"
#include "command.hpp"
#include "utils/conversion.hpp"

using namespace std;

namespace redox {

using redox::Redox;
using redox::Command;

class RedoxHash {
public:
  /**
   *  Constructor
   *
   * @param rd an instance of the RedoxClient (already connected)
   * @param hash_key the key name for the hash
   * @return string
  **/
  RedoxHash(Redox &rd,const std::string &hash_key) {
     rdx = &rd;
     key = hash_key;
  }

  /**
   *  Destructor
   **/
  ~RedoxHash() {
     rdx =NULL;
  }

  /**
   * Get the Hash key
   *
   * @return string
  **/
  std::string getKey()
  {
    return key;
  }

  /**
   * Redis HASH set command - synchronous
   *
   * @param field hash field
   * @param value value to set
   *
   * @return true if value set, otherwise false meaning the value existed and
   *         it's updated
   **/
  template <typename T>
  bool hset(const std::string& field, const T& value);

  /**
   * Redis HASH set command - asynchronous
   *
   * @param field hash field
   * @param value value to set
   * @param callback called when reply arrives
   *
   * @return true if value set, otherwise false meaning the value existed and
   *         it's updated
   **/
  template <typename T>
  void hset(const std::string& field, const T& value,
	    const std::function<void(Command<int>&)> &callback);

  /**
   * Redis HASH set if doesn't exist command - synchronous
   *
   * @param field hash field
   * @param value value to set
   *
   * @return true if value set, otherwise false meaning the value existed and
   *         no operation was performed
   **/
  template <typename T>
  bool hsetnx(const std::string& field, const T& value);

  /**
   * Redis HASH del command - synchronous
   *
   * @param field hash field
   *
   * @return true if value set, otherwise false meaning the value existed and
   *         it's updated
   **/
  bool hdel(const std::string& field);

  /**
   * Redis HASH del command - asynchronous
   *
   * @param field hash field
   * @param callback called when reply arrives
   **/
  void hdel(const std::string& field,
	    const std::function<void(Command<int> &)> &callback);

  /**
   * Redis HASH get command  - synchronous
   *
   * @param field hash field
   *
   * @return return the value associated with "field" in the hash stored at "key".
   *         If no such key exists then it return an empty string
   **/
  std::string hget(const std::string& field);

  /**
   * Redis HASH get all command - synchronous
   *
   * @return list of fields and their values stored in the hash, or an empty
   *         list when key does not exist
   **/
  std::vector<std::string> hgetall();

 /**
   * Redis HASH exists command - synchronous
   *
   * @param field hash field
   *
   * @return true if hash "key" contains "field", otherwise false
   **/
  bool hexists(const std::string& field);

  /**
   * Redis HASH length command - synchronous
   *
   * @return number of fields in the hash, or 0 if key does not exists
   **/
  long long int hlen();

  /**
   * Redis HASH length command - synchronous
   *
   * @param callaback called when reply arrives
   **/
  void hlen(const std::function<void(Command<long long int> &)> &callback);

  /**
   * Redis HASH increment_by command - synchronous
   *
   * @param key name of the hash
   * @param field hash field
   * @param increment value to increment by
   *
   * @return the value at "field" after the increment operation
   **/
  template <typename T>
  long long int
  hincrby(const std::string& field, const T& increment);

  /**
   * Redis HASH increment_by_float command - synchronous
   *
   * @param field hash field
   * @param increment value to increment by
   *
   * @return the value at "field" after the increment operation
   **/
  template <typename T>
  double
  hincrbyfloat(const std::string& field, const T& increment);

  /**
   * Redis HASH keys command - synchronous
   *
   * @return vector of fields in the hash
   **/
  std::vector<std::string> hkeys();

  /**
   * Redis HASH values command - synchronous
   *
   * @return vector of values in the hash
   **/
  std::vector<std::string> hvals();

  /**
   * Redis HASH SCAN command - synchronous
   *
   * @param cursor cursor for current request
   * @param count max number of elements to return
   *
   * @return pair representing the cursor value and a map of the elements
   *         returned in the current step
   **/
  std::pair< long long, std::unordered_map<std::string, std::string> >
  hscan(long long cursor, long long count = 1000);

private:
  Redox * rdx; ///< Redox client object
  std::string key; ///< Key of the hash object
};

//------------------------------------------------------------------------------
// Hash related templated methods implementation
//------------------------------------------------------------------------------
template <typename T>
bool RedoxHash::hset(const std::string& field, const T& value) {
  std::string svalue = utils::stringify(value);
  Command<int>& c = rdx->commandSync<int>({"HSET", key, field, svalue});

  if (!c.ok()) {
    throw std::runtime_error("FATAL] Error hset key: " + key + " field: "
			     + field + ": Status code " + std::to_string(c.status()));
  }

  int reply = c.reply();
  c.free();
  return (reply == 1);
}

template <typename T>
void RedoxHash::hset(const std::string& field, const T& value,
		 const std::function<void(Command<int>&)>& callback) {
  std::string svalue = utils::stringify(value);
  (void) rdx->command<int>({"HSET", key, field, svalue}, callback);
}

template <typename T>
bool RedoxHash::hsetnx(const std::string& field, const T& value) {
  std::string svalue = utils::stringify(value);
  Command<int>& c = rdx->commandSync<int>({"HSETNX", key, field, svalue});

  if (!c.ok()) {
    throw std::runtime_error("FATAL] Error hset key: " + key + " field: " +
			     field + ": Status code " + std::to_string(c.status()));
  }

  int reply = c.reply();
  c.free();
  return (reply == 1);
}

template <typename T> long long int
RedoxHash::hincrby(const std::string& field, const T& increment) {
  std::string sincrement = utils::stringify(increment);
  Command<long long int>& c
    = rdx->commandSync<long long int>({"HINCRBY", key, field, sincrement});

  if (!c.ok()) {
    throw std::runtime_error("FATAL] Error hincrby key: " + key + " field: " +
			     field + " value: " + sincrement + ": Status code "
			     + std::to_string(c.status()));
  }

  long long int reply = c.reply();
  c.free();
  return reply;
}

template <typename T> double
RedoxHash::hincrbyfloat(const std::string& field, const T& increment) {
  std::string sincrement = utils::stringify(increment);
  Command<std::string>& c
    = rdx->commandSync<std::string>({"HINCRBYFLOAT", key, field, sincrement});

  if (!c.ok()) {
    throw std::runtime_error("FATAL] Error hincrbyfloat key: " + key + " field:"
			     + field + ": Status code " + std::to_string(c.status()));
  }

  std::string reply = c.reply();
  c.free();
  return std::stod(reply);
}
} // namespace redox
