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
#include <string>

namespace redox {
namespace utils {

  /**
   * Helper function specialization to convert to std::string if necessary
   *
   * @param s string value
   * @return string
   **/
  std::string stringify(std::string s);

  /**
   * Helper function to convert to std::string if necessary
   *
   * @param s input value that can be converted to string
   * @return string representatino of the input
   **/
  template <typename T>
  typename std::enable_if < !std::is_convertible<T, std::string>::value,
			    std::string >::type
  stringify(T&& value) {
    using std::to_string; // take advantage of ADL (argument-dependent lookup)
    return to_string(std::forward<T> (value)); // exploit perfect forwarding
  }
}
}
