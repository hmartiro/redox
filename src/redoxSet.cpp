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

#include "redoxSet.hpp"

using namespace std;

namespace redox {

//------------------------------------------------------------------------------
// Redis SET add command for multiple members - synchronous
//------------------------------------------------------------------------------
long long int RedoxSet::sadd(std::vector<std::string> vect_members) {
  (void) vect_members.insert(vect_members.begin(), key);
  (void) vect_members.insert(vect_members.begin(), "SADD");
  Command<long long int> &c = rdx->commandSync<long long int>(vect_members);

  if (!c.ok()) {
    throw runtime_error("[FATAL] Error adding members to set " + key +
			": Status code " + to_string(c.status()));
  }

  long long int reply = c.reply();
  c.free();
  return reply;
}

//------------------------------------------------------------------------------
// Redis SET remove command for multiple members - synchronous
//------------------------------------------------------------------------------
  long long int RedoxSet::srem(std::vector<std::string> vect_members) {
  (void) vect_members.insert(vect_members.begin(), key);
  (void) vect_members.insert(vect_members.begin(), "SREM");
  Command<long long int> &c = rdx->commandSync<long long int>(vect_members);

  if (!c.ok()) {
    throw runtime_error("[FATAL] Error removing members from set " + key +
			": Status code " + to_string(c.status()));
  }

  long long int reply = c.reply();
  c.free();
  return reply;
}

//------------------------------------------------------------------------------
// Redis SET size command - synchronous
//------------------------------------------------------------------------------
long long int RedoxSet::scard() {
  Command<long long int> &c = rdx->commandSync<long long int>({"SCARD", key});

  if (!c.ok()) {
    throw runtime_error("[FATAL] Error getting number of members for set "
			+ key + ": Status code " + to_string(c.status()));
  }

  int reply = c.reply();
  c.free();
  return reply;
}

//------------------------------------------------------------------------------
// Redis SET smembers command - synchronous
//------------------------------------------------------------------------------
std::set<std::string> RedoxSet::smembers() {
  Command< std::set<std::string> > &c =
    rdx->commandSync< std::set<std::string> >({"SMEMBERS", key});

  if (!c.ok()) {
    throw runtime_error("[FATAL] Error getting members for set " + key +
			": Status code " + to_string(c.status()));
  }

  std::set<std::string> reply = c.reply();
  c.free();
  return reply;
}

//------------------------------------------------------------------------------
// Redis SET SCAN command - synchronous
//------------------------------------------------------------------------------
std::pair< long long, std::vector<std::string> >
RedoxSet::sscan(long long cursor, long long count) {
  Command<redisReply*> &c =  rdx->commandSync<redisReply*>(
    {"SSCAN", key, std::to_string(cursor), "COUNT", std::to_string(count)});

  if (!c.ok()) {
    throw runtime_error("[FATAL] Error executing SSCAN for set " + key +
			": Status code " + to_string(c.status()));
  }

  // Parse the Redis reply
  redisReply* reply = c.reply();
  long long new_cursor = std::stoll({reply->element[0]->str,
	static_cast<unsigned int>(reply->element[0]->len)});

  // First element is the new cursor
  std::pair<long long, std::vector<std::string> > retc_pair;
  retc_pair.first = new_cursor;
  reply = reply->element[1]; // move to the array part of the response

  for (unsigned long i = 0; i < reply->elements; ++i) {
    retc_pair.second.emplace_back(reply->element[i]->str,
				  static_cast<unsigned int>(reply->element[i]->len));
  }

  c.free();
  return retc_pair;
}
} // namespace redox
