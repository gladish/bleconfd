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
#include "rpcserver.h"
#include "rpclogger.h"

#ifdef WITH_BLUEZ
#include "bluez/gattServer.h"
#endif

#include <stdarg.h>

extern "C" RpcService* AppSettings_Create();
extern "C" RpcService* WiFiService_Create();
extern "C" RpcService* NetService_Create();

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

  struct RpcMethodInfo
  {
    std::string ServiceName;
    std::string MethodName;
  };

  RpcMethodInfo
  parseMethod(char const* s)
  {
    RpcMethodInfo methodInfo;
    char const* p = strchr(s, '-');
    if (p)
    {
      methodInfo.ServiceName = std::string(s, (p-s));
      methodInfo.MethodName = std::string(p + 1);
    }
    return methodInfo;
  }
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
    std::shared_ptr<RpcService>(AppSettings_Create()),
    std::shared_ptr<RpcService>(WiFiService_Create()),
    std::shared_ptr<RpcService>(NetService_Create())
  };
  return services;
}

RpcService::RpcService()
{
}

RpcService::~RpcService()
{
}

BasicRpcService::BasicRpcService(std::string const& name)
  : RpcService()
  , m_name(name)
{
}

BasicRpcService::~BasicRpcService()
{
}

std::string
BasicRpcService::name() const
{
  return m_name;
}

std::vector<std::string>
BasicRpcService::methodNames() const
{
  std::vector<std::string> names;
  for (auto itr : m_methods)
    names.push_back(itr.first);
  return names;
}

void
BasicRpcService::registerMethod(std::string const& name, RpcMethod const& method)
{
  m_methods.insert(std::make_pair(name, method));
}

cJSON*
BasicRpcService::makeError(int code, char const* format, ...)
{
  va_list ap;
  va_start(ap, format);

  int n = vsnprintf(0, 0, format, ap);
  va_end(ap);

  char* buff = new char[n + 1 ];
  buff[n] = '\0';

  va_start(ap, format);
  n = vsnprintf(buff, n + 1, format, ap);

  cJSON* res = cJSON_CreateObject();
  cJSON_AddItemToObject(res, "code", cJSON_CreateNumber(code));
  cJSON_AddItemToObject(res, "message", cJSON_CreateString(buff));

  delete [] buff;

  return res;
}

cJSON*
BasicRpcService::notImplemented(char const* methodName)
{
  return makeError(ENOENT, "method %s not found", methodName);
}

cJSON*
BasicRpcService::invokeMethod(std::string const& name, cJSON const* req)
{
  if (!req)
  {
    XLOG_ERROR("cannot invoke method %s-%s with null request",
      m_name.c_str(), name.c_str());
    // TODO: return error
    return nullptr;
  }

  XLOG_INFO("invoke method:%s-%s", m_name.c_str(), name.c_str());

  char* s = cJSON_Print(req);
  if (s)
  {
    XLOG_INFO("%s", s);
    free(s);
  }

  auto itr = m_methods.find(name);
  if (itr == m_methods.end())
  {
    XLOG_WARN("method %s-%s not found", m_name.c_str(), name.c_str());
    // TODO: return error
    return nullptr;
  }

  return itr->second(req);
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
    RpcMethodInfo methodInfo = parseMethod(method->valuestring);

    auto service = m_services.find(methodInfo.ServiceName);
    if (service == m_services.end())
    {
      // TODO
      XLOG_ERROR("failed to find service:%s", methodInfo.ServiceName.c_str());
      return;
    }
    else
    {
      cJSON* res = service->second->invokeMethod(methodInfo.MethodName, req);
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
  m_services.insert(std::make_pair(service->name(), service));
  service->init(m_config_file, callback);
}
