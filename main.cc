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
#include "beacon.h"
#include "appSettings.h"
#include "util.h"

#include <getopt.h>
#include <unistd.h>

#include <fstream>
#include <streambuf>
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <queue>
#include <condition_variable>

using std::string;
using std::vector;

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

  /**
   * get device line and split into vector from devices_db file
   **/
  vector<string> 
  getDeviceInfoFromDB(string const& rCode)
  {
    std::ifstream mf("devices_db");
    string line;


    while (std::getline(mf, line))
    {
      vector<string> t = split(line, ",");
      if (!t[0].compare(rCode))
      {
        return t;
      }
    }
    vector<string> empty;
    return empty;
  }

  /**
   * get the file content
   **/
  std::string getContentFromFile(char const* file)
  {
    std::ifstream mf(file);
    std::string system_id;
    if (std::getline(mf, system_id)) 
    {
      return system_id;
    } else 
    {
      return std::string("unkown");
    }
  }
  
  std::string DIS_getSystemId()
  {
    // get system id from machine id
    return getContentFromFile("/etc/machine-id");
  }

  std::string DIS_getModelNumber()
  {
    return getContentFromFile("/proc/device-tree/model");
  }

  std::string DIS_getSerialNumber()
  {
    return PROC_getVariable("/proc/cpuinfo", "Serial");
  }

  std::string DIS_getFirmwareRevision()
  {
    // get version from command "uname -a"
    std::string full = runCommand("uname -a");
    size_t index = full.find(" SMP");
    if (index != std::string::npos)
    {
      return full.substr(0, index);
    }
    return full;
  }

  std::string DIS_getHardwareRevision()
  {
    return PROC_getVariable("/proc/cpuinfo", "Revision");
  }

  std::string DIS_getSoftwareRevision()
  {
    return std::string(BLECONFD_VERSION);
  }

  std::string DIS_getManufacturerName()
  {
    string rCode = DIS_getHardwareRevision();
    vector<string> deviceInfo = getDeviceInfoFromDB(rCode);
    
    if( deviceInfo.size() > 0 )
    {
      return deviceInfo[4];
    }
    return std::string("unkown");
  }

  /**
   * dump Provider lines
   **/
  void DIS_dumpProvider()
  {
    XLOG_DEBUG("DIS_getSystemId         = %s", DIS_getSystemId().c_str());
    XLOG_DEBUG("DIS_getModelNumber      = %s", DIS_getModelNumber().c_str());
    XLOG_DEBUG("DIS_getSerialNumber     = %s", DIS_getSerialNumber().c_str());
    XLOG_DEBUG("DIS_getFirmwareRevision = %s", DIS_getFirmwareRevision().c_str());
    XLOG_DEBUG("DIS_getHardwareRevision = %s", DIS_getHardwareRevision().c_str());
    XLOG_DEBUG("DIS_getSoftwareRevision = %s", DIS_getSoftwareRevision().c_str());
    XLOG_DEBUG("DIS_getManufacturerName = %s", DIS_getManufacturerName().c_str());
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
      XLOG_INFO("creating dispatch thread");
      m_dispatch_thread.reset(new std::thread([this] { this->processIncomingQueue(); }));
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

    // for outgoing messages
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

    // this is called by the gatt server after reading a complete command
    // from the client. 
    void onIncomingMessage(cJSON* req)
    {
      XLOG_INFO("enqueue new incoming request");
      std::lock_guard<std::mutex> guard(m_mutex);
      m_incoming_queue.push(req);
      m_cond.notify_all();
    }

    void processIncomingQueue()
    {
      while (true)
      {
        cJSON* req = nullptr;
        XLOG_INFO("processing incoming queue");

        {
          std::unique_lock<std::mutex> guard(m_mutex);
          m_cond.wait(guard, [this] { return !this->m_incoming_queue.empty(); });

          if (!m_incoming_queue.empty())
          {
            req = m_incoming_queue.front();
            m_incoming_queue.pop();
          }
        }

        if (req)
          processRequest(req);
      }
    }

    void processRequest(cJSON* req)
    {
      XLOG_INFO("processing new incoming request");
      cJSON_Deleter req_deleter(req);

      cJSON* response_envelope = nullptr;

      response_envelope = cJSON_CreateObject();
      cJSON_AddItemToObject(response_envelope, "jsonrpc", cJSON_CreateString(JSON_RPC_VERSION));

      cJSON* method = cJSON_GetObjectItem(req, "method");
      if (!method)
      {
        XLOG_ERROR("request doesn't contain method");
        // TODO: respond with bad request
      }

      cJSON* id = cJSON_GetObjectItem(req, "id");
      if (!id)
      {
        XLOG_ERROR("request doesn't contain id");
        // TODO: respond with bad request
      }

      cJSON_AddItemToObject(response_envelope, "id", cJSON_CreateNumber(id->valueint));

      try
      {
        jsonRpcFunction func = jsonRpc_findFunction(method->valuestring);
        if (func)
        {
          cJSON* rpc_response = nullptr;
          XLOG_INFO("found %s and executing", method->valuestring);

          int ret = func(req, &rpc_response);

          XLOG_INFO("%s returned:%d", method->valuestring, ret);
          cJSON_AddItemToObject(response_envelope, "code", cJSON_CreateNumber(ret));
          if (ret)
          {
            cJSON_AddItemToObject(response_envelope, "error", rpc_response);
          }
          else
          {
            cJSON_AddItemToObject(response_envelope, "result", rpc_response);
          }
          // cJSON_Delete(rpc_response);
        }
        else
        {
          XLOG_ERROR("failed to find registered function %s", method->valuestring);
        }

        char* s = cJSON_Print(response_envelope);
        XLOG_INFO("response:%s", s);
        free(s);

        std::lock_guard<std::mutex> guard(m_mutex);
        if (m_client)
        {
          m_client->enqueueForSend(response_envelope);
        }
      }
      catch (std::exception const& err)
      {
        XLOG_ERROR("failed to queue response for send. %s", err.what());
      }
      if (response_envelope)
        cJSON_Delete(response_envelope);
    }

    void registerRpcFunctions()
    {
      jsonRpc_insertFunction("wifi-connect", wpaControl_connectToNetwork);
      jsonRpc_insertFunction("wifi-get-status", wpaControl_getStatus);
      jsonRpc_insertFunction("app-settings-get-all", appSettings_get_all);
      jsonRpc_insertFunction("app-settings-get", appSettings_get);
      jsonRpc_insertFunction("app-settings-set", appSettings_set);
      jsonRpc_insertFunction("app-settings-delete", appSettings_delete);
      jsonRpc_insertFunction("app-settings-group-add", appSettings_add_group);
      jsonRpc_insertFunction("app-settings-group-modify", appSettings_modify_group);
      jsonRpc_insertFunction("app-settings-group-delete", appSettings_delete_group);
    }

  private:
    std::shared_ptr<GattClient>   m_client; 
    std::mutex                    m_mutex;
    std::shared_ptr<std::thread>  m_dispatch_thread;
    std::queue<cJSON *>           m_incoming_queue;
    std::condition_variable       m_cond;
  };
}

void*
run_test(void* argp)
{
  sleep(2);

  RpcDispatcher* dispatcher = (RpcDispatcher *) argp;

  cJSON* req = cJSON_CreateObject();
  cJSON_AddItemToObject(req, "jsonrpc", cJSON_CreateString("2.0"));
  cJSON_AddItemToObject(req, "method", cJSON_CreateString("wifi-get-status"));
  cJSON_AddItemToObject(req, "id", cJSON_CreateNumber(1234));
  dispatcher->onIncomingMessage(req);

  return NULL;
}



int main(int argc, char* argv[])
{
  std::string configFile = "bleconfd.ini";
  xLog::getLogger().setLevel(logLevel_Info);

  while (true)
  {
    static struct option longOptions[] = 
    {
      { "config", required_argument, 0, 'c' },
      { "debug",  no_argument, 0, 'd' },
      { 0, 0, 0, 0 }
    };

    int optionIndex = 0;
    int c = getopt_long(argc, argv, "c:d", longOptions, &optionIndex);
    if (c == -1)
      break;

    switch (c)
    {
      case 'c':
        configFile = optarg;
        break;
      case 'd':
        xLog::getLogger().setLevel(logLevel_Debug);
      default:
        break;
    }
  }

  appSettings_init(configFile.c_str());
  
  RpcDispatcher rpc_dispatcher;
  rpc_dispatcher.registerRpcFunctions();

  int wpa_init_ret = wpaControl_init(appSettings_get_wifi_value("interface"), 
    std::bind(&RpcDispatcher::enqueueAsyncMessage,
    &rpc_dispatcher, std::placeholders::_1));

  if (wpa_init_ret)
  {
    XLOG_ERROR("wpaControl_init failed, code = %d", wpa_init_ret);
    exit(1);
  }

  std::string bleName(appSettings_get_ble_value("ble_name"));
  startBeacon(bleName);

  // TODO: add command line to run various tests
//   pthread_t thread;
//  pthread_create(&thread, nullptr, &run_test, &rpc_dispatcher);

  DIS_dumpProvider();
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

  wpaControl_shutdown();
  return 0;
}
