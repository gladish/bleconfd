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
int jsonRpc_getInt(cJSON const* argv, int idx);
int jsonRpc_getInt(cJSON const* argv, char const* name, bool required);

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

/**
 * Convenience function for returning errors in getters/setters
 * @param result [out] This will return a cJSON object in JSON-RPC format.
 * @param code The error code
 * @param fmt, The printf-style format.
 * @returns The code for easy use.
 */
int jsonRpc_makeError(cJSON** result, int code, char const* fmt, ...)
  __attribute__((format (printf, 3, 4)));

int jsonRpc_makeResult(cJSON** result, cJSON* value);

int jsonRpc_makeResultValue(cJSON** result, int code, char const* fmt, ...)
  __attribute__((format (printf, 3, 4)));

int jsonRpc_notImplemented(cJSON** result);


int jsonRpc_result(int n, cJSON** result);
int jsonRpc_resultBool(int n, unsigned char& b, cJSON** result);
int jsonRpc_resultInt(int ret, int& n, cJSON** result);
int jsonRpc_resultUnsignedInt(int ret, unsigned int& n, cJSON** result);
void jsonRpc_ok(cJSON** result);

#endif
