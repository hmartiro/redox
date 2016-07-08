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

#include "utils/conversion.hpp"

/**
 * Helper function specialization to convert to std::string if necessary
 **/
std::string redox::utils::stringify(std::string s) {
  // use implicit conversion to std::string
  return std::move(s); // take advantage of move semantics
}
