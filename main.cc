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
#include "rpclogger.h"
#include "rpcserver.h"

#include <getopt.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <cJSON.h>

void*
run_test(void* argp)
{
  sleep(2);

  RpcServer* server = (RpcServer*) argp;

  cJSON* req = cJSON_CreateObject();
  cJSON_AddItemToObject(req, "jsonrpc", cJSON_CreateString("2.0"));
//  cJSON_AddItemToObject(req, "method", cJSON_CreateString("wifi-get-status"));
  cJSON_AddItemToObject(req, "method", cJSON_CreateString("wifi-scan"));
//  cJSON_AddItemToObject(req, "method", cJSON_CreateString("rpc-list-methods"));
  cJSON_AddItemToObject(req, "id", cJSON_CreateNumber(1234));

  cJSON* params = cJSON_CreateObject();
  cJSON_AddItemToObject(params, "band", cJSON_CreateString("24"));
//  cJSON_AddItemToObject(params, "service", cJSON_CreateString("wifi"));
  cJSON_AddItemToObject(req, "params", params);

  char* s = cJSON_PrintUnformatted(req);
  int n = strlen(s);
  server->onIncomingMessage(s, n);
  free(s);

  return NULL;
}

std::vector< std::shared_ptr<RpcService> > services();

int main(int argc, char* argv[])
{
  std::string configFile = "bleconfd.ini";
  RpcLogger::logger().setLevel(RpcLogLevel::Info);

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
        RpcLogger::logger().setLevel(RpcLogLevel::Debug);
      default:
        break;
    }
  }

  RpcServer server(configFile);
  for (auto const& service : services())
    server.registerService(service);

  #if 0
  pthread_t thread;
  pthread_create(&thread, nullptr, &run_test, &server);
  while (true)
  {
    sleep(1);
  }
  #endif

  while (true)
  {
    try
    {
      std::shared_ptr<RpcListener> listener(RpcListener::create());
      listener->init();

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

  return 0;
}
