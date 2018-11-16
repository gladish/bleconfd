/*
 * Copyright [2017] [Comcast, Corp.]
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "jsonRpc.h"
#include "logger.h"

#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef std::map< std::string, jsonRpcFunction > jsonRpcFunctionMap;
static jsonRpcFunctionMap jsonRpcFunctions;

int
jsonRpc_makeError(cJSON** result, int code, char const* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);

  int n = vsnprintf(0, 0, fmt, ap);
  va_end(ap);

  char* buff = new char[n + 1 ];
  buff[n] = '\0';

  va_start(ap, fmt);
  n = vsnprintf(buff, n + 1, fmt, ap);

  *result = cJSON_CreateObject();
  cJSON_AddItemToObject(*result, "code", cJSON_CreateNumber(code));
  cJSON_AddItemToObject(*result, "message", cJSON_CreateString(buff));

  delete [] buff;

  return code;
}

int 
jsonRpc_makeResult(cJSON** result, cJSON* value)
{
  *result = value;
  return 0;
}

int
jsonRpc_makeResultValue(cJSON** result, int code, char const* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);

  int n = vsnprintf(0, 0, fmt, ap);
  va_end(ap);

  char* buff = new char[2048 + 1 ];
  buff[n] = '\0';

  va_start(ap, fmt);
  n = vsnprintf(buff, n + 1, fmt, ap);

  *result = cJSON_CreateObject();
  cJSON_AddItemToObject(*result, "code", cJSON_CreateNumber(code));
  cJSON_AddItemToObject(*result, "message", cJSON_CreateString(buff));

  delete[] buff;

  return code;
}

jsonRpcFunction
jsonRpc_findFunction(char const* name)
{
  jsonRpcFunction func = NULL;
  jsonRpcFunctionMap::const_iterator itr = jsonRpcFunctions.find(name);
  if (itr != jsonRpcFunctions.end())
    func = itr->second;
  return func;
}

bool
jsonRpc_insertFunction(char const* name, jsonRpcFunction func)
{
  std::pair< jsonRpcFunctionMap::iterator, bool > p =
    jsonRpcFunctions.insert(std::pair<std::string, jsonRpcFunction>(name, func));
  if (!p.second)
  {
    XLOG_ERROR("%s already exists in jsonRpcFunctions registration map", name);
  }
  return p.second;
}

int
jsonRpc_getInt(cJSON* argv, int idx)
{
  return jsonRpc_getn(argv, idx)->valueint;
}

char const*
jsonRpc_getString(cJSON const* req, char const* name, bool required)
{
  char* s = NULL;

  cJSON* params = cJSON_GetObjectItem(req, "params");
  if (!params && required)
  {
    std::stringstream buff;
    buff << "missing 'params' structure in request";
    throw std::runtime_error(buff.str());
  }

  cJSON* item = cJSON_GetObjectItem(params, name);

  if (!item && required)
  {
    std::stringstream buff;
    buff << "missing argument ";
    buff << name;
    buff << " from argv list";
    throw std::runtime_error(buff.str());
  }

  s = item->valuestring;
  if (!s || (strcmp(s, "<null>") == 0))
    s = NULL;

  return s;
}

char const*
jsonRpc_getString(cJSON* argv, int idx)
{
  char* s = NULL;
  char* t = jsonRpc_getn(argv, idx)->valuestring;
  if (t && (strcmp(t, "<null>") != 0))
    s = t;
  return s;
}

bool
jsonRpc_getBool(cJSON* argv, int idx)
{
  int t = jsonRpc_getn(argv, idx)->type;
  if (t == cJSON_True)
    return true;
  if (t == cJSON_False)
    return false;
  XLOG_ERROR("expected JSON bool, but got: %d", t);
  assert(false);
}

cJSON *
jsonRpc_getn(cJSON* argv, int idx)
{
  int const n = cJSON_GetArraySize(argv);
  if (n <= idx)
  {
    std::stringstream buff;
    buff << "not enough args (";
    buff << n << " <= " << idx;
    buff << ")";
    throw std::runtime_error(buff.str());
  }

  cJSON* arg = cJSON_GetArrayItem(argv, idx);
  if (!arg)
  {
    std::stringstream buff;
    buff << "null argument " << idx;
    throw std::runtime_error(buff.str());
  }
  return arg;
}

unsigned long
jsonRpc_getULong(cJSON* argv, int idx)
{
  double d = jsonRpc_getn(argv, idx)->valuedouble;
  return static_cast<unsigned long>(d);
}

static bool
jsonExtractArg(cJSON* obj, char const* spec, void* p)
{
  bool extracted = false;

  char type;
  char name[64];
  char const* end;
  char const* begin;
  int length = -1;

  begin = spec;
  end = strchr(spec, ':');

  memset(name, 0, sizeof(name));
  strncpy(name, begin, (end - begin));

  obj = cJSON_GetObjectItem(obj, name);
  if (!obj)
  {
    char* str = cJSON_Print(obj);
    XLOG_WARN("failed to find:%s in %s", name, str);
    free(str);
    return false;
  }

  begin = end + 1;
  type = *begin++;

  bool unsignedModifier = false;
  if (*begin)
  {
    if (*begin == 'u')
    {
      unsignedModifier = true;
    }
    if (type == 's')
      length = static_cast<int>(strtol(begin, NULL, 10));
  }

  switch (type)
  {
    case 's':
    {
      char* dest = reinterpret_cast<char *>(p);
      if (length)
        strncpy(dest, obj->valuestring, length);
      else
        strcpy(dest, obj->valuestring);
      extracted = true;
    }
    break;

    case 'l':
    {
      if (unsignedModifier)
      {
        unsigned long* lu = reinterpret_cast<unsigned long *>(p);
        *lu = static_cast<unsigned long>(obj->valuedouble);
      }
      else
      {
        long* l = reinterpret_cast<long *>(p);
        *l = static_cast<long>(obj->valuedouble);
      }
      extracted = true;
    }
    break;

    case 'b':
    {
      bool* b = reinterpret_cast<bool *>(p);
      *b = (obj->type == cJSON_True) ? true : false;
      extracted = true;
    }
    break;

    case 'f':
    {
      float* f = reinterpret_cast<float *>(p);
      *f = static_cast<float>(obj->valuedouble);
      extracted = true;
    }
    break;

    case 'u':
    {
      uint32_t* u = reinterpret_cast<uint32_t *>(p);
      *u = static_cast<uint32_t>(obj->valueint);
      extracted = true;
    }
    break;

    case 'd':
    {
      int32_t* i = reinterpret_cast<int32_t *>(p);
      *i = obj->valueint;
      extracted = true;
    }
    break;
  }

  return extracted;
}

int
jsonRpc_scanf(cJSON* obj, char const* fmt, ...)
{
  int n = 0;

  va_list ap;
  va_start(ap, fmt);

  char fmtSpec[2048];

  while (*fmt)
  {
    char const* begin = fmt;
    while (*begin && *begin != '{')
      begin++;

    if (*begin == '\0')
      break;

    char const* end = begin++;
    while (end && *end != '}')
      end++;

    memset(fmtSpec, 0, sizeof(fmtSpec));
    strncpy(fmtSpec, begin, (end - begin));

    if (jsonExtractArg(obj, fmtSpec, va_arg(ap, void *)))
      n++;

    fmt = end;
  }

  va_end(ap);
  return n;
}

int
jsonRpc_binaryEncode(uint8_t const* buff, size_t len, std::string& encoded)
{
  // TODO: should do base64
  std::ostringstream out; 
  for (size_t i = 0; i < len; ++i)
  {
    out << std::hex;
    out << std::setfill('0');
    out << std::setw(2);
    out << static_cast<int>(buff[i]);
    if (i < (len -1))
      out << ' ';
  }
  encoded = out.str();
  return 0;
}

int
jsonRpc_binaryDecode(char const* s, std::vector<uint8_t>& decoded)
{
  // TODO: should do base64
  std::istringstream in(s);
  in.clear();

  while (!in.eof())
  {
    int i;
    in >> std::hex >> i;
    decoded.push_back(static_cast<uint8_t>(i));
  }

  return 0;
}

int
jsonRpc_resultInt(int ret, int& n, cJSON** result)
{
  if (ret == RETURN_OK)
    *result = cJSON_CreateNumber(n);
  else
    ret = jsonRpc_makeError(result, ret, "failed");
  return ret;
}

void
jsonRpc_ok(cJSON** result)
{
  jsonRpc_makeResult(result, cJSON_CreateString("ok"));
}

int
jsonRpc_result(int ret, cJSON** result)
{
  if (ret == RETURN_OK)
    jsonRpc_ok(result);
  else
    jsonRpc_makeError(result, ret, "failed");
  return ret;
}

#if 0
jsonRpcStringBuffer::jsonRpcStringBuffer()
{
  m_buff = new char[REMOTEAPI_STRING_MAX_SAFE];
  memset(m_buff, 0, REMOTEAPI_STRING_MAX_SAFE);
}

jsonRpcStringBuffer::~jsonRpcStringBuffer()
{
  delete [] m_buff;
}

int
jsonRpc_resultString(int ret, jsonRpcStringBuffer& buff, cJSON** result)
{
  if (ret == RETURN_OK)
    *result = cJSON_CreateString(buff);
  else
    ret = jsonRpc_makeError(result, ret, "failed");
  return ret;
}
#endif

int
jsonRpc_resultBool(int ret, unsigned char& b, cJSON** result)
{
  bool val = (b != 0 ? true : false);
  if (ret == RETURN_OK)
    *result = cJSON_CreateBool(val);
  else
    ret = jsonRpc_makeError(result, ret, "failed");
  return ret;
}

int
jsonRpc_resultUnsignedInt(int ret, unsigned int& n, cJSON** result)
{
  if (ret == RETURN_OK)
    *result = cJSON_CreateNumber(n);
  else
    ret = jsonRpc_makeError(result, ret, "failed");
  return ret;
}

int
jsonRpc_notImplemented(cJSON** result)
{
  return jsonRpc_makeError(result, ENOENT, "method not implementd");
}
