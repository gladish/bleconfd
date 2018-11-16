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
#include "appSettings.h"
#include "xLog.h"

#include <getopt.h>

static void rpcServer_init();
static void rpcServer_dispatch(cJSON const* req, cJSON** res);

// TODO: proper test framework
static cJSON* Test_wifiConnect();
static cJSON* Test_wifiStatus();
static cJSON* Test_appSettingsGet();
static cJSON* Test_appSettingsSet();

int main(int argc, char* argv[])
{
  std::string configFile = "bleconfd.ini";

  while (true)
  {
    static struct option longOptions[] = 
    {
      { "config", required_argument, 0, 'c' },
      { 0, 0, 0, 0 }
    };

    int optionIndex = 0;
    int c = getopt_long(argc, argv, "c:", longOptions, &optionIndex);
    if (c == -1)
      break;

    switch (c)
    {
      case 'c':
        configFile = optarg;
        break;
      default:
        break;
    }
  }

  xLog::getLogger().setLevel(logLevel_Debug);

  rpcServer_init();
  wpaControl_init("/var/run/wpa_supplicant/wlan1");
  appSettings_init(configFile.c_str());

  // TODO: read this from BLE inbox

  cJSON* req = Test_appSettingsGet();
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
  if (!res)
  {
    XLOG_ERROR("null request object");
    // TODO
  }

  char* s = cJSON_Print(req);
  if (s)
  {
    XLOG_DEBUG("incoming request\n\t%s", s);
    free(s);
  }

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
    cJSON* temp = cJSON_CreateObject();
    cJSON_AddItemToObject(temp, "jsonrpc", cJSON_CreateString("2.0"));
    cJSON_AddItemToObject(temp, "id", cJSON_CreateNumber(id->valueint));

    int ret = func(req, &response);
    if (ret)
      cJSON_AddItemToObject(temp, "error", response);
    else
      cJSON_AddItemToObject(temp, "result", response);

    *res = temp;
  }
  else
  {
    XLOG_ERROR("failed to find registered function %s", method->valuestring);
  }
}

void rpcServer_init()
{
  jsonRpc_insertFunction("wifi-connect", wpaControl_connectToNetwork);
  jsonRpc_insertFunction("wifi-get-status", wpaControl_getStatus);
  jsonRpc_insertFunction("app-settings-get", appSettings_get);
  jsonRpc_insertFunction("app-settings-set", appSettings_set);
}

cJSON* Test_wifiConnect()
{
  cJSON* req = cJSON_CreateObject();
  cJSON_AddItemToObject(req, "jsonrpc", cJSON_CreateString("2.0"));
  cJSON_AddItemToObject(req, "id", cJSON_CreateNumber(2));
  cJSON_AddItemToObject(req, "method", cJSON_CreateString("wifi-connect"));
  cJSON_AddItemToObject(req, "wi-fi_tech", cJSON_CreateString("infra"));

  cJSON* disco = cJSON_CreateObject();
  cJSON_AddItemToObject(disco, "ssid", cJSON_CreateString("JAKE_5"));
  cJSON_AddItemToObject(req, "discovery", disco);

  cJSON* creds = cJSON_CreateObject();
  cJSON_AddItemToObject(creds, "akm", cJSON_CreateString("psk"));
  cJSON_AddItemToObject(creds, "pass", cJSON_CreateString("jake1234"));
  cJSON_AddItemToObject(req, "cred", creds);

  return req;
}

cJSON* Test_wifiStatus()
{
  cJSON* req = cJSON_CreateObject();
  cJSON_AddItemToObject(req, "jsonrpc", cJSON_CreateString("2.0"));
  cJSON_AddItemToObject(req, "method", cJSON_CreateString("wifi-get-status"));
  cJSON_AddItemToObject(req, "id", cJSON_CreateNumber(1234));

  return req;
}

cJSON* Test_appSettingsGet()
{
  cJSON* req = cJSON_CreateObject();
  cJSON_AddItemToObject(req, "jsonrpc", cJSON_CreateString("2.0"));
  cJSON_AddItemToObject(req, "method", cJSON_CreateString("app-settings-get"));
  cJSON_AddItemToObject(req, "id", cJSON_CreateNumber(1));

  cJSON* params = cJSON_CreateObject();
  cJSON_AddItemToObject(params, "name", cJSON_CreateString("setting1"));
  cJSON_AddItemToObject(req, "params", params);
  return req;
}

cJSON* Test_appSettingsSet()
{
  cJSON* req = cJSON_CreateObject();
  cJSON_AddItemToObject(req, "jsonrpc", cJSON_CreateString("2.0"));
  cJSON_AddItemToObject(req, "method", cJSON_CreateString("app-settings-set"));
  cJSON_AddItemToObject(req, "id", cJSON_CreateNumber(2));

  cJSON* params = cJSON_CreateObject();
  cJSON_AddItemToObject(params, "name", cJSON_CreateString("setting1"));
  cJSON_AddItemToObject(params, "value", cJSON_CreateNumber(2));
  cJSON_AddItemToObject(params, "type", cJSON_CreateNumber(appSettingsKind_Int32));
  cJSON_AddItemToObject(req, "params", params);
  return req;
}
