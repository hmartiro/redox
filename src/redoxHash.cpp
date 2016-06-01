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

#include "redoxHash.hpp"

using namespace std;

namespace redox {

//------------------------------------------------------------------------------
// Redis HASH del command  - synchronous
//------------------------------------------------------------------------------
bool
RedoxHash::hdel(const std::string& field) {
  Command<int>& c = rdx->commandSync<int>({"HDEL", key, field});

  if (!c.ok()) {
    throw std::runtime_error("[FATAL] Error hdel key: " + key + " field: " + field +
			     ": Status code " + std::to_string(c.status()));
  }

  int reply = c.reply();
  c.free();
  return (reply == 1);
}

//------------------------------------------------------------------------------
// Redis HASH del command - asynchronous
//------------------------------------------------------------------------------
void
RedoxHash::hdel(const std::string& field,
		const std::function<void(Command<int> &)> &callback) {
  (void) rdx->command<int>({"HDEL", key, field}, callback);
}

//------------------------------------------------------------------------------
// Redis HASH get command - synchronous
//------------------------------------------------------------------------------
std::string
RedoxHash::hget(const std::string& field) {
  Command<std::string>& c = rdx->commandSync<std::string>({"HGET", key, field});

  if (!c.ok()) {
    return std::string();
  }

  std::string reply = c.reply();
  c.free();
  return reply;
}

//------------------------------------------------------------------------------
// Redis HASH get all command - synchronous
//------------------------------------------------------------------------------
std::vector<std::string>
RedoxHash::hgetall() {
  Command< std::vector<std::string> >& c =
    rdx->commandSync< std::vector<std::string> >({"HGETALL", key});

  if (!c.ok()) {
    return std::vector<std::string>();
  }

  std::vector<std::string> reply = c.reply();
  c.free();
  return reply;
}

//------------------------------------------------------------------------------
// Redis HASH exists command - synchronous
//------------------------------------------------------------------------------
bool
RedoxHash::hexists(const std::string& field) {
  Command<int>& c = rdx->commandSync<int>({"HEXISTS", key, field});

  if (!c.ok()) {
    throw std::runtime_error("[FATAL] Error hexists key: " + key + " field: "
			     + field + ": Status code " +
			     std::to_string(c.status()));
  }

  int reply = c.reply();
  c.free();
  return (reply == 1);
}

//------------------------------------------------------------------------------
// Redis HASH length command - synchronous
//------------------------------------------------------------------------------
long long int
RedoxHash::hlen() {
  Command<long long int>& c = rdx->commandSync<long long int>({"HLEN", key});

  if (!c.ok()) {
    throw std::runtime_error("[FATAL] Error hlen key: " + key + ": Status code "
			     + std::to_string(c.status()));
  }

  long long int reply = c.reply();
  c.free();
  return reply;
}

//------------------------------------------------------------------------------
// Redis HASH length command - asynchronous
//------------------------------------------------------------------------------
void
RedoxHash::hlen(const std::function<void(Command<long long int> &)> &callback)
{
  (void) rdx->command<long long int>({"HLEN", key}, callback);
}

//------------------------------------------------------------------------------
// Redis HASH keys command - synchronous
//------------------------------------------------------------------------------
std::vector<std::string>
RedoxHash::hkeys() {
  Command< std::vector<std::string> >& c
    = rdx->commandSync< std::vector<std::string> >({"HKEYS", key});

  if (!c.ok()) {
    throw std::runtime_error("[FATAL] Error hkeys key: " + key + ": Status code "
			     + std::to_string(c.status()));
  }

  std::vector<std::string> vect_resp = c.reply();
  c.free();
  return vect_resp;
}

//------------------------------------------------------------------------------
// Redis HASH keys command - synchronous
//------------------------------------------------------------------------------
std::vector<std::string>
RedoxHash::hvals() {
  Command< std::vector<std::string> >& c
    = rdx->commandSync< std::vector<std::string> >({"HVALS", key});

  if (!c.ok()) {
    throw std::runtime_error("[FATAL] Error hvals key: " + key + ": Status code "
			     + std::to_string(c.status()));
  }

  std::vector<std::string> vect_resp = c.reply();
  c.free();
  return vect_resp;
}

//------------------------------------------------------------------------------
// Redis HASH SCAN command - synchronous
//------------------------------------------------------------------------------
std::pair< long long, std::unordered_map<std::string, std::string> >
RedoxHash::hscan(long long cursor, long long count) {
  Command<redisReply*> &c =  rdx->commandSync<redisReply*>(
    {"HSCAN", key, std::to_string(cursor), "COUNT", std::to_string(count)});

  if (!c.ok()) {
    throw runtime_error("[FATAL] Error executing HSCAN for map " + key +
			": Status code " + to_string(c.status()));
  }

  // Parse the Redis reply
  redisReply* reply = c.reply();
  long long new_cursor = std::stoll({reply->element[0]->str,
	static_cast<unsigned int>(reply->element[0]->len)});

  // First element is the new cursor
  std::pair<long long, std::unordered_map<std::string, std::string> > retc_pair;
  retc_pair.first = new_cursor;
  reply = reply->element[1]; // move to the array part of the response

  for (unsigned long i = 0; i < reply->elements; i += 2) {
    retc_pair.second.emplace(
      std::string(reply->element[i]->str,
		  static_cast<unsigned int>(reply->element[i]->len)),
      std::string(reply->element[i + 1]->str,
		  static_cast<unsigned int>(reply->element[i + 1]->len)));
  }

  c.free();
  return retc_pair;
}
}
