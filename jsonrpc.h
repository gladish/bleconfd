//
// Copyright [2018] [Comcast NBCUniversal]
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
#ifndef __JSON_RPC_H__
#define __JSON_RPC_H__

#include <cJSON.h>
#include <string>

class JsonRpc
{
public:
  static cJSON* wrapResponse(int code, cJSON* res, int reqId);
  static cJSON* makeError(int code, char const* fmt, ...) __attribute__((format (printf, 2, 3)));
  static cJSON* notImplemented(char const* methodName);

  static int
  getInt(
    cJSON const*  json,
    char const*   name,
    bool          required = false,
    int           defaultValue = 0);

  static char const*
  getString(
    cJSON const*  json,
    char const*   name,
    bool          required,
    char const*   defaultValue = nullptr);

  static std::string
  getStringWithExpansion(
    cJSON const*  json,
    char const*   name,
    bool          required,
    char const*   defaultValue = nullptr,
    cJSON const*  replacements = nullptr);

  static cJSON const* search(cJSON const* json, char const* name, bool required);
};

#endif
