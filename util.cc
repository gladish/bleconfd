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
#include "util.h"

std::vector <std::string>
split(std::string const& str, std::string const& delim)
{
  std::vector <std::string> tokens;
  size_t prev = 0, pos = 0;
  do
  {
    pos = str.find(delim, prev);
    if (pos == std::string::npos) pos = str.length();
    std::string token = str.substr(prev, pos - prev);
    if (!token.empty()) tokens.push_back(token);
    prev = pos + delim.length();
  } while (pos < str.length() && prev < str.length());
  return tokens;
}


std::string runCommand(char const* command)
{
  FILE* fp;
  char buffer[256];
  fp = popen(command, "r");
  if (fp == NULL)
  {
    return std::string("unkown");
  }
  fgets(buffer, sizeof(buffer) - 1, fp);
  pclose(fp);
  return std::string(buffer);
}