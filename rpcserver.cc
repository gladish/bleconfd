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
#include "defs.h"
#include "rpcserver.h"
#include "xLog.h"

#ifdef WITH_BLUEZ
#include "bluez/gattServer.h"
#endif

#include "appSettings.h"
#include "wpaControl.h"

namespace
{
  class JsonDeleter
  {
  public:
    JsonDeleter(cJSON* json) : m_json(json) { }
    ~JsonDeleter()
    {
      if (m_json)
        cJSON_Delete(m_json);
    }
  private:
    cJSON* m_json;
  };

}

std::shared_ptr<RpcListener>
RpcListener::create()
{
  return std::shared_ptr<RpcListener>(
#ifdef WITH_BLUEZ
    new GattServer()
#endif
  );
}

std::vector< std::shared_ptr<RpcService> >
services()
{
  std::vector< std::shared_ptr<RpcService> > services
  {
    std::shared_ptr<RpcService>(new AppSettingsService),
    std::shared_ptr<RpcService>(new WiFiService)
  };
  return services;
}


RpcServer::RpcServer(std::string const& configFile)
  : m_config_file(configFile)
{
  m_dispatch_thread.reset(new std::thread([this] { this->processIncomingQueue(); }));
}

RpcServer::~RpcServer()
{
}

void
RpcServer::setClient(std::shared_ptr<RpcConnectedClient> const& client)
{
  std::lock_guard<std::mutex> guard(m_mutex);
  m_client = client;
}

void
RpcServer::stop()
{
  std::lock_guard<std::mutex> guard(m_mutex);
  m_client.reset();
}

void
RpcServer::run()
{
  m_client->run();
  stop();
}

void
RpcServer::enqueueAsyncMessage(cJSON const* json)
{
  if (!json)
    return;
  {
    char* s = cJSON_PrintUnformatted(json);
    int n = static_cast<int>(strlen(s));

    std::lock_guard<std::mutex> guard(m_mutex);
    if (m_client)
      m_client->enqueueForSend(s, n);
    free(s);
  }
}

void
RpcServer::onIncomingMessage(char const* s, int UNUSED_PARAM(n))
{
  XLOG_INFO("enqueue new incoming request");
  std::lock_guard<std::mutex> guard(m_mutex);

  cJSON* req = cJSON_Parse(s);
  if (req)
  {
    m_incoming_queue.push(req);
    m_cond.notify_all();
  }
  else
  {
    //TODO:
    XLOG_ERROR("failed to parse incoming json request");
  }
}

void
RpcServer::processIncomingQueue()
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
    {
      JsonDeleter requestDeleter(req);
      processRequest(req);
    }
  }
}

void
RpcServer::processRequest(cJSON const* req)
{
  XLOG_INFO("processing new incoming request");

  cJSON* envelope = nullptr;

  envelope = cJSON_CreateObject();
  cJSON_AddItemToObject(envelope, "jsonrpc", cJSON_CreateString(kJsonRpcVersion));
  JsonDeleter envelopDeleter(envelope);


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

  cJSON_AddItemToObject(envelope, "id", cJSON_CreateNumber(id->valueint));

  try
  {
    RpcMethod func = nullptr;
    auto itr = m_methods.find(method->valuestring);
    if (itr != m_methods.end())
    {
      func = itr->second;
    }
    else
    {
      XLOG_ERROR("failed to find registered function '%s'", method->valuestring);
    }

    if (func)
    {
      XLOG_INFO("found %s and executing", method->valuestring);

      cJSON* res = func(req);

      if (res)
      {
        cJSON_AddItemToObject(envelope, "result", res);
      }
      else
      {
        // TODO:
        // cJSON_AddItemToObject(response_envelope, "error", temp);
      }
    }

    char* s = cJSON_Print(envelope);
    XLOG_INFO("response:%s", s);
    free(s);

    std::lock_guard<std::mutex> guard(m_mutex);
    if (m_client)
    {
      char* s = cJSON_PrintUnformatted(envelope);
      int n = strlen(s);
      m_client->enqueueForSend(s, n);
      free(s);
    }
  }
  catch (std::exception const& err)
  {
    XLOG_ERROR("failed to queue response for send. %s", err.what());
  }
}

void
RpcServer::registerService(std::shared_ptr<RpcService> const& service)
{
  XLOG_INFO("registering service:%s", service->name().c_str());
  RpcNotificationFunction callback = std::bind(&RpcServer::enqueueAsyncMessage, this,
    std::placeholders::_1);

  for (auto const& m: service->methodNames())
    registerMethodForService(service, m, service->method(m));
  service->init(m_config_file, callback);
}

void
RpcServer::registerMethodForService(std::shared_ptr<RpcService> const& service,
  std::string const& methodName, RpcMethod const& method)
{
  std::stringstream buff;
  buff << service->name();
  buff << '-';
  buff << methodName;
  std::string name = buff.str();

  if (method == nullptr)
  {
    XLOG_WARN("can't register null method for:%s", name.c_str());
    return;
  }


  XLOG_INFO("\tregistering method:%s", name.c_str());
  m_methods.insert(std::make_pair(name.c_str(), method));
}
