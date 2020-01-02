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
#include "defs.h"
#include "rpclogger.h"
#include "rpcserver.h"
#include "jsonrpc.h"
#include "util.h"

#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <fstream>
#include <mutex>
#include <string>
#include <cJSON.h>

std::string DIS_getSystemId();
std::string DIS_getModelNumber();
std::string DIS_getSerialNumber();
std::string DIS_getFirmwareRevision();
std::string DIS_getHardwareRevision();
std::string DIS_getSoftwareRevision();
std::string DIS_getManufacturerName();
std::string DIS_getContentFromFile(char const* fname);
std::string DIS_getVariable(char const* fname, char const* field);
std::vector<std::string> DIS_getDeviceInfoFromDB(std::string const& rCode);

static std::string getFullPath(std::string const& f)
{
  char buff[1024] = {0};
  char* s = realpath(f.c_str(), buff);
  return (s != nullptr ? std::string(s) : f);
}

class SignalingConnectedClient : public RpcConnectedClient
{
public:
  SignalingConnectedClient()
    : RpcConnectedClient()
    , m_have_response(false) { }
  virtual ~SignalingConnectedClient() { }
  virtual void init(DeviceInfoProvider const& UNUSED_PARAM(deviceInfoProvider)) override { }
  virtual void enqueueForSend(char const* UNUSED_PARAM(buff), int UNUSED_PARAM(n)) override
  {
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_have_response = true;
    }
    m_cond.notify_one();
  }
  virtual void run() override
  {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cond.wait(lock, [this] {return this->m_have_response;});
  }
  virtual void setDataHandler(RpcDataHandler const& UNUSED_PARAM(handler)) override { }
private:
  std::mutex              m_mutex;
  std::condition_variable m_cond;
  bool                    m_have_response;
};

void
printHelp()
{
  printf("\n");
  printf("bleconfd [args]\n");
  printf("\t-c  --config <file> Confuguration file\n");
  printf("\t-d  --debug         Enable debug logging\n");
  printf("\t-t  --test   <file> Read json file, exec, and exit\n");
  printf("\t-h  --help          Print this help and exit\n");
  exit(0);
}

int main(int argc, char* argv[])
{
  cJSON* testInput = nullptr;
  std::string configFile = "bleconfd.json";
  RpcLogger::logger().setLevel(RpcLogLevel::Info);


  while (true)
  {
    static struct option longOptions[] = 
    {
      { "config", required_argument, 0, 'c' },
      { "debug",  no_argument, 0, 'd' },
      { "test",   required_argument, 0, 't' },
      { "help",   no_argument, 0, 'h' },
      { 0, 0, 0, 0 }
    };

    int optionIndex = 0;
    int c = getopt_long(argc, argv, "c:dt:", longOptions, &optionIndex);
    if (c == -1)
      break;

    switch (c)
    {
      case 'c':
        configFile = optarg;
        break;
      case 'd':
        RpcLogger::logger().setLevel(RpcLogLevel::Debug);
        break;
      case 't':
        testInput = JsonRpc::fromFile(optarg);
        break;
      case 'h':
        printHelp();
        break;
      default:
        break;
    }
  }

  XLOG_INFO("loading configuration from file %s", getFullPath(configFile).c_str());
  cJSON* config = JsonRpc::fromFile(configFile.c_str());
  if (!config)
  {
    XLOG_ERROR("failed to load configuration file from:%s\n", configFile.c_str());
    return 1;
  }

  RpcServer server(configFile, config);
  XLOG_INFO("rpc server intialized");

  if (testInput)
  {
    std::shared_ptr<RpcConnectedClient> client(new SignalingConnectedClient());
    server.setClient(client);

    XLOG_INFO("starting test runner thread");
    std::thread testRunner([&] {
//      std::this_thread::sleep_for(std::chrono::seconds(2));
      char* s = cJSON_PrintUnformatted(testInput);
      server.onIncomingMessage(s, strlen(s));
      free(s);
    });
    testRunner.join();
    server.run();
  }
  else
  {
    DeviceInfoProvider deviceInfoProvider;
    deviceInfoProvider.GetSystemId = &DIS_getSystemId;
    deviceInfoProvider.GetModelNumber = &DIS_getModelNumber;
    deviceInfoProvider.GetSerialNumber = &DIS_getSerialNumber;
    deviceInfoProvider.GetFirmwareRevision = &DIS_getFirmwareRevision;
    deviceInfoProvider.GetHardwareRevision = &DIS_getHardwareRevision;
    deviceInfoProvider.GetSoftwareRevision = &DIS_getSoftwareRevision;
    deviceInfoProvider.GetManufacturerName = &DIS_getManufacturerName;

    cJSON const* listenerConfig = cJSON_GetObjectItem(config, "listener");

    while (true)
    {
      try
      {
        std::shared_ptr<RpcListener> listener(RpcListener::create());
        listener->init(listenerConfig);

        // blocks here until remote client makes BT connection
        std::shared_ptr<RpcConnectedClient> client = listener->accept(deviceInfoProvider);
        client->setDataHandler(std::bind(&RpcServer::onIncomingMessage,
              &server, std::placeholders::_1, std::placeholders::_2));
        server.setClient(client);
        server.run();
      }
      catch (std::runtime_error const& err)
      {
        XLOG_ERROR("unhandled exception:%s", err.what());
        return -1;
      }
    }
  }

  return 0;
}

std::string
DIS_getSystemId()
{
  return DIS_getContentFromFile("/etc/machine-id");
}

std::string
DIS_getModelNumber()
{
  std::string s = DIS_getContentFromFile("/proc/device-tree/model");
  size_t n = s.size();
  if (s[n - 1] == '\0')
    s = s.substr(0, (n-1));
  return s;
}

std::string
DIS_getSerialNumber()
{
  std::string s = DIS_getVariable("/proc/cpuinfo", "Serial");
  XLOG_INFO("s:%s", s.c_str());
  return s;
}

std::string
DIS_getFirmwareRevision()
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

std::string
DIS_getHardwareRevision()
{
  return DIS_getVariable("/proc/cpuinfo", "Revision");
}

std::string
DIS_getSoftwareRevision()
{
  return std::string(BLECONFD_VERSION);
}

std::string
DIS_getManufacturerName()
{
  std::string rCode = DIS_getHardwareRevision();
  std::vector<std::string> deviceInfo = DIS_getDeviceInfoFromDB(rCode);

  if (deviceInfo.size())
  {
    return deviceInfo[4];
  }
  return std::string("unkown");
}

std::string
DIS_getContentFromFile(char const* fname)
{
  std::string id;

  std::ifstream infile(fname);
  if (!std::getline(infile, id))
    id = "unknown";

  if (id[id.size()] == '\0')
    id = id.substr(0, id.size() - 1);

  return id;
}

std::string
DIS_getVariable(char const* file, char const* field)
{
  char buff[256];
  FILE* f = fopen(file, "r");
  while (fgets(buff, sizeof(buff) - 1, f) != nullptr)
  {
    if (strncmp(buff, field, strlen(field)) == 0)
    {
      char* p = strchr(buff, ':');
      if (p)
      {
        p++;
        while (p && isspace(*p))
          p++;
      }
      if (p)
      {
        size_t len = strlen(p);
        if (len > 0)
          p[len -1] = '\0';
        return std::string(p);
      }
    }
  }
  return std::string();
}

std::vector<std::string>
DIS_getDeviceInfoFromDB(std::string const& rCode)
{
  std::ifstream mf("devices_db");
  std::string line;

  while (std::getline(mf, line))
  {
    std::vector<std::string> t = split(line, ",");
    if (!t[0].compare(rCode))
      return t;
  }

  return std::vector<std::string>();
}
