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

#include <string>
#include <vector>

#include <stdint.h>
#include <cJSON.h>

#ifndef RETURN_OK
#define RETURN_OK   0
#endif

#ifndef RETURN_ERR
#define RETURN_ERR   -1
#endif

#define APIREG(NAME) jsonRpc_insertFunction(#NAME, remote_ ## NAME)

#define ADDNUM(FIELD) cJSON_AddItemToObject(o, #FIELD, cJSON_CreateNumber(stats.FIELD))
#define ADDSTR(FIELD) cJSON_AddItemToObject(o, #FIELD, cJSON_CreateString(stats.FIELD))
#define ADDPNUM(FIELD) cJSON_AddItemToObject(o, #FIELD, cJSON_CreateNumber(stats->FIELD))
#define ADDPSTR(FIELD) cJSON_AddItemToObject(o, #FIELD, cJSON_CreateString(stats->FIELD))

/**
 *
 */
typedef int (*jsonRpcFunction)(cJSON const* args, cJSON** result);

/**
 *
 */
jsonRpcFunction jsonRpc_findFunction(char const* name);


/**
 *
 */
bool jsonRpc_insertFunction(char const* name, jsonRpcFunction func);

/**
 *
 */
char const* jsonRpc_getString(cJSON const* req, char const* name, bool required);

/**
 *
 */
bool jsonRpc_getBool(cJSON* argv, int idx);

/**
 *
 */
cJSON* jsonRpc_getn(cJSON const* argv, int idx);

/**
 *
 */
unsigned long jsonRpc_getULong(cJSON* argv, int idx);

/**
 *
 */
int jsonRpc_scanf(cJSON* obj, char const* fmt, ...);

/**
 *
 */
int jsonRpc_binaryEncode(uint8_t const* buff, size_t len, std::string& encoded);
int jsonRpc_binaryDecode(char const* s, std::vector<uint8_t>& decoded);

class JsonRpc
{
public:
  static cJSON* wrapResponse(cJSON* res, int reqId);
  static cJSON* makeError(int code, char const* fmt, ...) __attribute__((format (printf, 2, 3)));

  static int getInt(cJSON const* json, char const* name, bool required);
  static char const* getString(cJSON const* json, char const* name, bool required);
};

#endif
