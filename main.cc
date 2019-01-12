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
#include "defs.h"
#include "rpclogger.h"
#include "rpcserver.h"
#include "jsonrpc.h"

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

class SignalingConnectedClient : public RpcConnectedClient
{
public:
  SignalingConnectedClient()
    : RpcConnectedClient()
    , m_have_response(false) { }
  virtual ~SignalingConnectedClient() { }
  virtual void init() override { }
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
      case 't':
        testInput = JsonRpc::fromFile(optarg);
        break;
      default:
        break;
    }
  }

  XLOG_INFO("loading configuration from file %s", configFile.c_str());
  cJSON* config = JsonRpc::fromFile(configFile.c_str());
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
    cJSON const* listenerConfig = cJSON_GetObjectItem(config, "listener");

    while (true)
    {
      try
      {
        std::shared_ptr<RpcListener> listener(RpcListener::create());
        listener->init(listenerConfig);

        // blocks here until remote client makes BT connection
        std::shared_ptr<RpcConnectedClient> client = listener->accept();
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
