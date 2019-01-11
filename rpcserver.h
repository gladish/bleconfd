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
#ifndef __RPC_SERVER_H__
#define __RPC_SERVER_H__

#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>


struct cJSON;
class RpcService;

using RpcDataHandler = std::function<void (char const* buff, int n)>;
using RpcNotificationFunction = std::function<void (cJSON const* json)>;
using RpcMethod = std::function<cJSON* (cJSON const* req)>;
using RpcMethodMap = std::map< std::string, RpcMethod >;
using RpcServiceConstructor = std::function<RpcService* ()>;


class RpcConnectedClient
{
public:
  RpcConnectedClient() { }
  virtual ~RpcConnectedClient() { }
  virtual void init() = 0;
  virtual void enqueueForSend(char const* buff, int n) = 0;
  virtual void run() = 0;
  virtual void setDataHandler(RpcDataHandler const& handler) = 0;
};

class RpcService
{
public:
  RpcService();
  virtual ~RpcService();
  virtual void init(cJSON const* conf, RpcNotificationFunction const& callback)  = 0;
  virtual std::string name() const = 0;
  virtual std::vector<std::string> methodNames() const = 0;
  virtual cJSON* invokeMethod(std::string const& name, cJSON const* req) = 0;

public:
  static void registerServiceConstructor(std::string const& name, RpcServiceConstructor const& ctor);
  static RpcService* createServiceByName(std::string const& name);
};

class RpcServiceRegistrar
{
public:
  RpcServiceRegistrar(std::string const& name, RpcServiceConstructor const& ctor);
};

#define JSONRPC_SERVICE_DEFINE(NAME, CTOR) static RpcServiceRegistrar __ ## NAME(#NAME, CTOR)

class BasicRpcService : public RpcService
{
public:
  BasicRpcService(std::string const& name);
  virtual ~BasicRpcService();
  virtual std::string name() const override;
  virtual std::vector<std::string> methodNames() const override;
  virtual cJSON* invokeMethod(std::string const& name, cJSON const* req) override;
  virtual void init(cJSON const* conf, RpcNotificationFunction const& callback) override;

protected:
  void registerMethod(std::string const& name, RpcMethod const& method);
  void notifyAndDelete(cJSON* json);

protected:
  cJSON*                  m_config;

private:
  RpcMethodMap            m_methods;
  std::string             m_name;
  RpcNotificationFunction m_notify;
};

class RpcListener
{
public:
  RpcListener() { }
  virtual ~RpcListener() { }
  virtual void init(cJSON const* conf) = 0;
  virtual std::shared_ptr<RpcConnectedClient> accept() = 0;

public:
  static std::shared_ptr<RpcListener> create();
};

class RpcServer
{
public:
  RpcServer(std::string const& configFile, cJSON const* config);
  ~RpcServer();

private:
  class RpcSystemService : public BasicRpcService
  {
  public:
    RpcSystemService(RpcServer* parent);
    virtual ~RpcSystemService();
    virtual void init(cJSON const* conf, RpcNotificationFunction const& callback) override;
  private:
    cJSON* listServices(cJSON const* req);
    cJSON* listMethods(cJSON const* req);
    cJSON* getServerPublicKey(cJSON const* req);
    cJSON* setClientPublicKey(cJSON const* req);
  private:
    RpcServer* m_server;
  };

  struct RpcMethodInfo
  {
    RpcMethodInfo() { }
    RpcMethodInfo(std::string const& service, std::string const& method)
      : ServiceName(service)
      , MethodName(method) { }
    std::string const ServiceName;
    std::string const MethodName;
    std::string toString() const;
    static RpcMethodInfo parseMethod(char const* s);
  };

  friend class RpcSystemService;

public:
  void setClient(std::shared_ptr<RpcConnectedClient> const& tport);
  void registerService(std::shared_ptr<RpcService> const& service);
  void stop();
  void run();
  void enqueueAsyncMessage(cJSON const* json);
  void onIncomingMessage(const char* buff, int n);
  void setLastChanceHandler(RpcMethod const& lastChanceHandler);

private:
  void processIncomingQueue();
  void processRequest(cJSON const* req);
  cJSON* processJsonRpcRequest(cJSON const* req);
  cJSON* processNonJsonRpcRequest(cJSON const* req);
  cJSON* invokeMethod(RpcMethodInfo const& methodInfo, cJSON const* req);

private:
  std::shared_ptr<RpcConnectedClient> m_client;
  std::mutex                          m_mutex;
  std::shared_ptr<std::thread>        m_dispatch_thread;
  std::queue<cJSON *>                 m_incoming_queue;
  std::condition_variable             m_cond;
  std::map< std::string, std::shared_ptr<RpcService> > m_services;
  cJSON*                              m_config;
  std::string                         m_config_file;
  RpcMethod                           m_last_chance;
};

// not sure where to put these
std::string chomp(char const* s);

#endif
