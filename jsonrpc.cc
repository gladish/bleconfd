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
#include "jsonrpc.h"
#include "rpclogger.h"
#include "defs.h"

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

namespace
{

}

cJSON*
JsonRpc::makeError(int code, char const* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);

  int n = vsnprintf(0, 0, fmt, ap);
  va_end(ap);

  char* buff = new char[n + 1 ];
  buff[n] = '\0';

  va_start(ap, fmt);
  n = vsnprintf(buff, n + 1, fmt, ap);

  cJSON* result = cJSON_CreateObject();
  cJSON_AddItemToObject(result, "code", cJSON_CreateNumber(code));
  cJSON_AddItemToObject(result, "message", cJSON_CreateString(buff));

  delete [] buff;

  return result;
}


cJSON const*
JsonRpc::search(cJSON const* json, char const* name, bool required)
{
  if (!json)
  {
    std::stringstream buff;
    buff << "null json object when fetching field";
    if (name)
      buff << ":" << name;
    throw std::runtime_error(buff.str());
  }

  if (!name || strlen(name) == 0)
  {
    std::stringstream buff;
    buff << "null name when fetching json field";
    throw std::runtime_error(buff.str());
  }

  cJSON const* item = nullptr;

  if (name[0] == '/')
  {
    char* str = strdup(name + 1);
    char* saveptr = nullptr;
    char* token = nullptr;

    item = json;
    while ((token = strtok_r(str, "/", &saveptr)) != nullptr)
    {
      if (token == nullptr)
        break;

      item = cJSON_GetObjectItem(item, token);
      if (!item)
        break;

      str = nullptr;
    }

    free(str);
  }
  else
  {
    item = cJSON_GetObjectItem(json, name);
  }

  if (!item && required)
  {
    std::stringstream buff;
    buff << "missing argument ";
    buff << name;
    buff << " from argv list";
    throw std::runtime_error(buff.str());
  }

  return item;
}

int
JsonRpc::getInt(cJSON const* req, char const* name, bool required, int defaultValue)
{
  int n = defaultValue;
  cJSON const* item = JsonRpc::search(req, name, required);
  if (item)
    n = item->valueint;
  return n;
}

char const*
JsonRpc::getString(cJSON const* req, char const* name, bool required, char const* defaultValue)
{
  char const* s = defaultValue;

  cJSON const* item = JsonRpc::search(req, name, required);

  if (!item && required)
  {
    std::stringstream buff;
    buff << "missing argument ";
    buff << name;
    buff << " from argv list";
    throw std::runtime_error(buff.str());
  }

  if (item)
    s = item->valuestring;

  if (!s || (strcmp(s, "<null>") == 0))
    s = NULL;

  return s;
}

#if 0
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
#endif

cJSON*
JsonRpc::wrapResponse(int code, cJSON* res, int reqId)
{
  cJSON* envelope = cJSON_CreateObject();
  cJSON_AddStringToObject(envelope, "jsonrpc", kJsonRpcVersion);
  if (reqId != -1)
    cJSON_AddNumberToObject(envelope, "id", reqId);
  if (code == 0)
    cJSON_AddItemToObject(envelope, "result", res);
  else
    cJSON_AddItemToObject(envelope, "error", res);
  return envelope;
}

cJSON*
JsonRpc::notImplemented(char const* methodName)
{
  return JsonRpc::makeError(ENOENT, "method %s not implemented", methodName);
}
