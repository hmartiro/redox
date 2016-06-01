/*
 * Redox - A modern, asynchronous, and wicked fast C++11 client for Redis
 *
 *    https://github.com/hmartiro/redox
 *
 * Copyright 2015 - Hayk Martirosyan <hayk.mart at gmail dot com>
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

namespace redox {

using redox::Redox;
using redox::Command;

class RedoxSet {
public:
  /**
   * Constructor
   *
   * @param rd an instance of the RedoxClient (already connected)
   * @param set_key the key name for the set
   * @return string
  **/
  RedoxSet(Redox &rd, const std::string &set_key) {
     rdx = &rd;
     key = set_key;
  }

  /**
   * Destructor
  **/
  ~RedoxSet() {
     rdx =NULL;
  }

  /**
   * Get the Set key
   *
   * @return string
  **/
  std::string getKey() {
    return key;
  }

  /**
   * Redis SET add command - synchronous
   *
   * @param member value to be added to the set
   *
   * @return true if member added, otherwise false
   **/
  template <typename T>
  bool sadd(const T& member);

  /**
   * Redis SET add command - asynchronous
   *
   * @param member value to be added to the set
   * @param callback called when reply arrives
   **/
  template <typename T>
  void sadd(const T& member,
	    const std::function<void(Command<int>&)>& callback);

  /**
   * Redis SET add command for multiple members - synchronous
   *
   * @param vect_members values to be added to the set
   * TODO: make vect_members rvalue ref to use move semantics
   *
   * @return number of elements added to the set
   **/
  // TODO: template the vector contents
  long long int sadd(std::vector<std::string> vect_members);

  /**
   * Redis SET remove command - synchronous
   *
   * @param member value to be removed from the set
   *
   * @return true if member removed, otherwise false
   **/
  template <typename T>
  bool srem(const T& member);

  /**
   * Redis SET remove command - asynchronous
   *
   * @param member value to be removed from the set
   * @param callback called when reply arrives
   *
   * @return true if member removed, otherwise false
   **/
  template <typename T>
  void srem( const T& member,
	    const std::function<void(Command<int>&)>& callback);

  /**
   * Redis SET remove command for multiple members - synchronous
   *
   * @param vect_members values to be removed from the set
   *
   * @return number of elements removed from the set
   **/
  // TODO: template the vector contents
  long long int srem(std::vector<std::string> vect_members);

  /**
   * Redis SET size command - synchronous
   *
   * @return size of the set
   **/
  long long int scard();

  /**
   * Redis SET ismember command - synchronous
   *
    @param member value to be searched in the set
   *
   * @return true if member in the set, otherwise false
   **/
  template <typename T>
  bool sismember(const T& member);

  /**
   * Redis SET members command - synchronous
   *
   * @return set containing the members of the set
   **/
  std::set<std::string> smembers();

  /**
   * Redis SET SSCAN command - synchronous
   *
   * @param cursor cursor for current request
   * @param count max number of elements to return
   *
   * @return pair representing the cursor value and a vector of the elements
   *         returned in the current step
   **/
  std::pair< long long, std::vector<std::string> >
  sscan( long long cursor, long long count = 1000);

private:
  Redox* rdx; ///< Redox client object
  std::string key; ///< Key of the set object
};

//------------------------------------------------------------------------------
// Set related templated methods implementation
//------------------------------------------------------------------------------
template <typename T>
bool RedoxSet::sadd(const T& member) {
  std::string smember = utils::stringify(member);
  Command<int> &c = rdx->commandSync<int>({"SADD", key, smember});

  if (!c.ok()) {
    throw std::runtime_error("[FATAL] Error adding " + smember + " to set " +
			     key + ": Status code " + std::to_string(c.status()));
  }

  int reply = c.reply();
  c.free();
  return (reply == 1);
}

template <typename T>
void RedoxSet::sadd(const T& member,
		    const std::function<void(Command<int>&)>& callback) {
  std::string smember = utils::stringify(member);
  (void) rdx->command<int>({"SADD", key, smember}, callback);
}

template <typename T>
bool RedoxSet::srem(const T& member) {
  std::string smember = utils::stringify(member);
  Command<int> &c = rdx->commandSync<int>({"SREM", key, smember});

  if (!c.ok()) {
    throw std::runtime_error("[FATAL] Error removing " + smember + " from set "
			     + key + ": Status code " + std::to_string(c.status()));
  }

  int reply = c.reply();
  c.free();
  return (reply == 1);
}

template <typename T>
void RedoxSet::srem(const T& member,
		    const std::function<void(Command<int>&)>& callback) {
  std::string smember = utils::stringify(member);
  (void) rdx->command<int>({"SREM", key, smember}, callback);
}

template <typename T>
bool RedoxSet::sismember(const T& member) {
  std::string smember = utils::stringify(member);
  Command<int> &c = rdx->commandSync<int>({"SISMEMBER", key, smember});

  if (!c.ok()) {
    throw std::runtime_error("[FATAL] Error checking " + smember + " in set "
			     + key + ": Status code " + std::to_string(c.status()));
  }

  int reply = c.reply();
  c.free();
  return (reply == 1);
}
} // namespace redox
