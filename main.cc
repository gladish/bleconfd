//
// Copyright [2018] [jacobgladish@yahoo.com]
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
#include "jsonRpc.h"
#include "wpaControl.h"
#include "logger.h"

#include <getopt.h>

static void rpcServer_init();
static void rpcServer_dispatch(cJSON const* req, cJSON** res);

int main(int argc, char* argv[])
{
  // TODO: take configuration file from command line (use getopt_long())

  rpcServer_init();
  wpaControl_init("/var/run/wpa_supplicant/wlan1");

  // TODO: read this from BLE inbox
  cJSON* req = cJSON_CreateObject();
  cJSON_AddItemToObject(req, "jsonrpc", cJSON_CreateString("2.0"));
  cJSON_AddItemToObject(req, "method", cJSON_CreateString("get-wifi-status"));
  cJSON_AddItemToObject(req, "id", cJSON_CreateNumber(1234));

  cJSON* res = nullptr;
  rpcServer_dispatch(req, &res);

  // TODO: post response on BLE outbox
  if (res)
  {
    char* s = cJSON_Print(res);
    printf("%s\n", s);
    free(s);
    cJSON_Delete(res);
  }
  cJSON_Delete(req);

  wpaControl_shutdown();
  return 0;
}

void rpcServer_dispatch(cJSON const* req, cJSON** res)
{
  cJSON* response = nullptr;
  cJSON* method = cJSON_GetObjectItem(req, "method");
  if (!method)
  {
    XLOG_ERROR("request doesn't contain method");
    // TODO
  }

  cJSON* id = cJSON_GetObjectItem(req, "id");
  if (!id)
  {
    XLOG_ERROR("request doesn't contain id");
    // TODO
  }

  jsonRpcFunction func = jsonRpc_findFunction(method->valuestring);
  if (func)
  {
    int ret = func(req, &response);
    if (ret)
    {
      // TODO
    }
    else
    {
      cJSON* temp = cJSON_CreateObject();
      cJSON_AddItemToObject(temp, "jsonrpc", cJSON_CreateString("2.0"));
      cJSON_AddItemToObject(temp, "id", cJSON_CreateNumber(id->valueint));
      cJSON_AddItemToObject(temp, "result", response);

      *res = temp;
    }
  }
  else
  {
    XLOG_ERROR("failed to find registered function %s", method->valuestring);
  }
}

void rpcServer_init()
{
  jsonRpc_insertFunction("set-wifi-connect", wpaControl_connectToNetwork);
  jsonRpc_insertFunction("get-wifi-status", wpaControl_getStatus);
}
