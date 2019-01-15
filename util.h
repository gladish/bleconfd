//
// Copyright [2018] [Comcast, Corp]
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#ifndef __UTIL_H__
#define __UTIL_H__

#include <vector>
#include <string>

/**
 * split string
 */
std::vector<std::string> split(std::string const& str, std::string const& delim);

/**
 * run system command
 */
std::string runCommand(char const* cmd);

#endif
