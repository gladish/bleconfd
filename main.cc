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
#include "appSettings.h"
#include "wpaControl.h"
#include "gattServer.h"
#include "xLog.h"

#include <getopt.h>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
  std::string PROC_getVariable(char const* file, char const* field)
  {
    std::string line;
    std::ifstream in(file);
    while (std::getline(in, line))
    {
      if (line.compare(0, strlen(field), field) == 0)
      {
        size_t idx = line.find(':');
        if (idx != std::string::npos)
          line = line.substr(idx + 1);

        idx = 0;
        while (idx < line.length() && line[idx] == ' ')
          line = line.substr(1);

        return line;
      }
    }

    return std::string();
  }

  std::string DIS_getSystemId()
  {
    // TODO
    return std::string();
  }

  std::string DIS_getModelNumber()
  {
    // TODO
    return std::string();
  }

  std::string DIS_getSerialNumber()
  {
    return PROC_getVariable("/proc/cpuinfo", "Serial");
  }

  std::string DIS_getFirmwareRevision()
  {
    // TODO
    return std::string();
  }

  std::string DIS_getHardwareRevision()
  {
    return PROC_getVariable("/proc/cpuinfo", "Revision");
  }

  std::string DIS_getSoftwareRevision()
  {
    return std::string();
  }

  std::string DIS_getManufacturerName()
  {
    // TODO
    return std::string();
  }

  GattClient::DeviceInfoProvider disProvider =
  {
    DIS_getSystemId,
    DIS_getModelNumber,
    DIS_getSerialNumber,
    DIS_getFirmwareRevision,
    DIS_getHardwareRevision,
    DIS_getSoftwareRevision,
    DIS_getManufacturerName
  };

  class cJSON_Deleter
  {
  public:
    cJSON_Deleter(cJSON* j) : m_json(j) { }
    ~cJSON_Deleter()
    {
      if (m_json)
        cJSON_Delete(m_json);
    }
  private:
    cJSON* m_json;
  };

  class RpcDispatcher
  {
  public:
    RpcDispatcher()
    {
    }

    void setClient(std::shared_ptr<GattClient> const& clnt)
    {
      std::lock_guard<std::mutex> guard(m_mutex);
      m_client = clnt;
      m_client->setDataHandler(std::bind(&RpcDispatcher::onIncomingMessage, this,
        std::placeholders::_1));
    }

    void stop()
    {
      std::lock_guard<std::mutex> guard(m_mutex);
      m_client.reset();
    }

    void run()
    {
      // blocks here until client disconnects
      m_client->run();
      stop();
    }

    void enqueueAsyncMessage(cJSON* json)
    {
      if (!json)
        return;
      {
        std::lock_guard<std::mutex> guard(m_mutex);
        if (m_client)
          m_client->enqueueForSend(json);
      }

      cJSON_Delete(json);
    }

    void onIncomingMessage(cJSON* req)
    {
      #if 0
      char* s = cJSON_Print(req);
      if (s)
      {
        XLOG_DEBUG("incoming request\n\t%s", s);
        free(s);
      }
      #endif
      cJSON_Deleter req_deleter(req);

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

      try
      {
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
        }
        else
        {
          XLOG_ERROR("failed to find registered function %s", method->valuestring);
        }

        std::lock_guard<std::mutex> guard(m_mutex);
        if (m_client)
          m_client->enqueueForSend(response);
      }
      catch (std::exception const& err)
      {
        XLOG_ERROR("failed to queue response for send. %s", err.what());
      }
    }

    void registerRpcFunctions()
    {
      jsonRpc_insertFunction("wifi-connect", wpaControl_connectToNetwork);
      jsonRpc_insertFunction("wifi-get-status", wpaControl_getStatus);
      jsonRpc_insertFunction("app-settings-get", appSettings_get);
      jsonRpc_insertFunction("app-settings-set", appSettings_set);
    }

  private:
    std::shared_ptr<GattClient> m_client; 
    std::mutex                  m_mutex;
  };
}

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

  RpcDispatcher rpc_dispatcher;
  rpc_dispatcher.registerRpcFunctions();

  wpaControl_init("/var/run/wpa_supplicant/wlan1", std::bind(&RpcDispatcher::enqueueAsyncMessage,
    &rpc_dispatcher, std::placeholders::_1));
  appSettings_init(configFile.c_str());

  while (true)
  {
    try
    {
      GattServer server;
      server.init();

      // blocks here until remote client makes BT connection
      std::shared_ptr<GattClient> clnt = server.accept(disProvider);
      rpc_dispatcher.setClient(clnt);
      rpc_dispatcher.run();
    }
    catch (std::runtime_error const& err)
    {
      XLOG_ERROR("unhandled exception:%s", err.what());
      return -1;
    }
  }

#if 0
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
#endif


  wpaControl_shutdown();
  return 0;
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
