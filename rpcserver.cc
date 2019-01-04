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
#include "jsonrpc.h"

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
    JsonDeleter(cJSON*& json) : m_json(json) { }
    ~JsonDeleter()
    {
      if (m_json)
        cJSON_Delete(m_json);
    }
  private:
    cJSON*& m_json;
  };

}

std::string
RpcServer::RpcMethodInfo::toString() const
{
  std::stringstream buff;
  buff << ServiceName;
  buff << '-';
  buff << MethodName;
  return buff.str();
}

RpcServer::RpcMethodInfo
RpcServer::RpcMethodInfo::parseMethod(char const* s)
{
  std::string service;
  std::string method;

  char const* p = strchr(s, '-');
  if (p)
  {
    service = std::string(s, (p-s));
    method = std::string(p + 1);
  }
  return RpcMethodInfo(service, method);
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

void
BasicRpcService::init(cJSON const* UNUSED_PARAM(config), RpcNotificationFunction const& callback)
{
  m_notify = callback;
}

void
BasicRpcService::notifyAndDelete(cJSON* json)
{
  if (!json)
    return;

  if (m_notify)
    m_notify(json);

  cJSON_Delete(json);
}

cJSON*
BasicRpcService::invokeMethod(std::string const& name, cJSON const* req)
{
  cJSON* res = nullptr;

  XLOG_INFO("invoke method:%s-%s", m_name.c_str(), name.c_str());

  if (!req)
  {
    XLOG_ERROR("cannot invoke method %s-%s with null request",
      m_name.c_str(), name.c_str());
    res = JsonRpc::makeError(-1, "no request object");
  }
  else
  {
    auto itr = m_methods.find(name);
    if (itr == m_methods.end())
    {
      XLOG_WARN("method %s-%s not found", m_name.c_str(), name.c_str());
      res = JsonRpc::makeError(-1, "method %s-%s not found", m_name.c_str(), name.c_str());
    }
    else
    {
      // actually invoke the method
      res = itr->second(req);
    }
  }

  return res;
}

RpcServer::RpcServer(cJSON const* config)
{
  if (config)
    m_config = cJSON_Duplicate(config, true);
  else
    m_config = nullptr;

  std::shared_ptr<RpcService> s(new IntrospectionService(this));
  registerService(s);

  m_dispatch_thread.reset(new std::thread([this] { this->processIncomingQueue(); }));
}

RpcServer::~RpcServer()
{
  if (m_config)
    cJSON_Delete(m_config);
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

  char* s = cJSON_PrintUnformatted(json);
  int n = static_cast<int>(strlen(s));

  XLOG_INFO("notify:%s", s);

  {
    std::lock_guard<std::mutex> guard(m_mutex);
    if (m_client)
      m_client->enqueueForSend(s, n);
    free(s);
  }
}

void
RpcServer::onIncomingMessage(char const* s, int UNUSED_PARAM(n))
{
  if (!s || strlen(s) == 0)
    return;

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

cJSON*
RpcServer::invokeMethod(RpcMethodInfo const& methodInfo, cJSON const* req)
{
  cJSON* res = nullptr;

  auto service = m_services.find(methodInfo.ServiceName);
  if (service == m_services.end())
  {
    res = JsonRpc::makeError(ENOENT, "service %s not found",
      methodInfo.ServiceName.c_str());
  }
  else
  {
    res = service->second->invokeMethod(methodInfo.MethodName, req);
    if (!res)
      res = JsonRpc::makeError(-1, "%s.%s returned null?",
        methodInfo.ServiceName.c_str(),
        methodInfo.MethodName.c_str());
  }

  return res;
}

void
RpcServer::processRequest(cJSON const* req)
{
  cJSON* res = nullptr;

  XLOG_INFO("processing new incoming request");
  {
    char* s = cJSON_Print(req);
    if (s)
    {
      XLOG_INFO("req:%s", s);
      free(s);
    }
  }

  // ensure json-rpc request
  if (!JsonRpc::getString(req, "jsonrpc", false, nullptr))
    res = processNonJsonRpcRequest(req);
  else
    res = processJsonRpcRequest(req);

  char* s = cJSON_Print(res);
  if (s)
  {
    XLOG_INFO("res:%s", s);
    {
      std::lock_guard<std::mutex> guard(m_mutex);
      if (m_client)
        m_client->enqueueForSend(s, strlen(s));
    }
    free(s);
  }
  else
  {
    XLOG_ERROR("failed to serialize JSON response to string");
  }

  cJSON_Delete(res);
}

cJSON*
RpcServer::processNonJsonRpcRequest(cJSON const* req)
{
  cJSON* res = nullptr;

  XLOG_INFO("incoming request is not jsonrpc, dispatching to last chance");
  if (m_last_chance)
  {
    try
    {
      res = m_last_chance(req);
    }
    catch (std::exception const& err)
    {
      res = JsonRpc::makeError(-1, "unhandled exception:%s", err.what());
    }
  }
  else
  {
    XLOG_WARN("invalid non-jsonrpc request with no last chance handler registered");
    res = JsonRpc::makeError(-1, "only jsonrpc is supported");
  }

  return res;
}

cJSON*
RpcServer::processJsonRpcRequest(cJSON const* req)
{
  cJSON* res = nullptr;

  cJSON* method = cJSON_GetObjectItem(req, "method");
  if (!method)
  {
    XLOG_ERROR("request doesn't contain method");
    res = JsonRpc::makeError(-1, "request doesn't contain a 'method'");
  }

  int requestId = -1;
  if (!res)
  {
    cJSON* id = cJSON_GetObjectItem(req, "id");
    if (!id)
    {
      XLOG_ERROR("request doesn't contain id");
      res = JsonRpc::makeError(-1, "request doesn't contain an 'id'");
    }
    else
    {
      requestId = id->valueint;
    }
  }

  if (!res)
  {
    try
    {
      res = invokeMethod(RpcMethodInfo::parseMethod(method->valuestring), req);
    }
    catch (std::exception const& err)
    {
      res = JsonRpc::makeError(-1, "unhandled exception:%s", err.what());
    }
  }

  // if function returned { "code": 1234, ... } where code != 0, then
  // it's an error, else it was ok. This is handled by the wrapResponse
  int code = JsonRpc::getInt(res, "code", false, 0);
  return JsonRpc::wrapResponse(code, res, requestId);
}

void
RpcServer::registerService(std::shared_ptr<RpcService> const& service)
{
  XLOG_INFO("registering service:%s", service->name().c_str());
  RpcNotificationFunction callback = std::bind(&RpcServer::enqueueAsyncMessage, this,
    std::placeholders::_1);
  m_services.insert(std::make_pair(service->name(), service));

  // TODO: someone update JsonRpc::search to handle lists so we can do
  // cJSON* conf = JsonRpc::search(m_conf, "/services/name/[@name='wifi']");
  cJSON* conf = nullptr;
  cJSON* serviceConfig = nullptr;
  if (m_config)
    serviceConfig = cJSON_GetObjectItem(m_config, "services");
  
  if (serviceConfig)
  {
    for (int i = 0, n = cJSON_GetArraySize(serviceConfig); i < n; ++i)
    {
      cJSON* temp = cJSON_GetArrayItem(serviceConfig, i);
      cJSON* name = cJSON_GetObjectItem(temp, "name");
      if (name && strcmp(name->valuestring, service->name().c_str()) == 0)
      {
        conf = temp;
        break;
      }
    }
  }

  service->init(conf, callback);
}

RpcServer::IntrospectionService::IntrospectionService(RpcServer* parent)
  : BasicRpcService("rpc")
  , m_server(parent)
{
}

RpcServer::IntrospectionService::~IntrospectionService()
{
}

void
RpcServer::IntrospectionService::init(cJSON const* UNUSED_PARAM(config),
  RpcNotificationFunction const& UNUSED_PARAM(callback))
{
  registerMethod("list-services", [this](cJSON const* req) -> cJSON* { return this->listServices(req); });
  registerMethod("list-methods", [this](cJSON const* req) -> cJSON* { return this->listMethods(req); });
}

cJSON*
RpcServer::IntrospectionService::listServices(cJSON const* UNUSED_PARAM(req))
{
  cJSON* res = cJSON_CreateObject();
  cJSON* names = cJSON_AddArrayToObject(res, "services");
  for (auto const& kv : m_server->m_services)
  {
    cJSON_AddItemToArray(names, cJSON_CreateString(kv.first.c_str()));
  }
  return res;
}

cJSON*
RpcServer::IntrospectionService::listMethods(cJSON const* req)
{
  cJSON* res = cJSON_CreateObject();
  cJSON const* service = JsonRpc::search(req, "/params/service", true);
  if (service)
  {
    cJSON* names = cJSON_AddArrayToObject(res, "methods");
    auto itr = m_server->m_services.find(service->valuestring);
    for (std::string const& s : itr->second->methodNames())
    {
      RpcMethodInfo methodInfo(service->valuestring, s);
      cJSON_AddItemToArray(names, cJSON_CreateString(methodInfo.toString().c_str()));
    }
  }
  return res;
}
