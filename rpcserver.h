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

using RpcDataHandler = std::function<void (char const* buff, int n)>;
using RpcNotificationFunction = std::function<void (cJSON const* json)>;

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
  virtual void init(std::string const& configFile, RpcNotificationFunction const& callback)  = 0;
  virtual std::string name() const = 0;
  virtual std::vector<std::string> methodNames() const = 0;
  virtual cJSON* invokeMethod(std::string const& name, cJSON const* req) = 0;
};

class BasicRpcService : public RpcService
{
public:
  BasicRpcService(std::string const& name);
  virtual ~BasicRpcService();
  virtual std::string name() const override;
  virtual std::vector<std::string> methodNames() const override;
  virtual cJSON* invokeMethod(std::string const& name, cJSON const* req) override;
  virtual void init(std::string const& configFile, RpcNotificationFunction const& callback) override;

protected:
  using RpcMethod = std::function<cJSON* (cJSON const* req)>;
  using RpcMethodMap = std::map< std::string, RpcMethod >;

  void registerMethod(std::string const& name, RpcMethod const& method);
  void notifyAndDelete(cJSON* json);

private:
  RpcMethodMap  m_methods;
  std::string   m_name;
  RpcNotificationFunction m_notify;
};

class RpcListener
{
public:
  RpcListener() { }
  virtual ~RpcListener() { }
  virtual void init(std::string const& name, std::string const& uuid) = 0;
  virtual std::shared_ptr<RpcConnectedClient> accept() = 0;

public:
  static std::shared_ptr<RpcListener> create();
};

class RpcServer
{
public:
  RpcServer(std::string const& configFile);
  ~RpcServer();

private:
  class IntrospectionService : public BasicRpcService
  {
  public:
    IntrospectionService(RpcServer* parent);
    virtual ~IntrospectionService();
    virtual void init(std::string const& configFile,
      RpcNotificationFunction const& callback) override;
  private:
    cJSON* listServices(cJSON const* req);
    cJSON* listMethods(cJSON const* req);
  private:
    RpcServer* m_server;
  };

  friend class IntrospectionService;

public:
  void setClient(std::shared_ptr<RpcConnectedClient> const& tport);
  void registerService(std::shared_ptr<RpcService> const& service);
  void stop();
  void run();
  void enqueueAsyncMessage(cJSON const* json);
  void onIncomingMessage(const char* buff, int n);

private:
  void processIncomingQueue();
  void processRequest(cJSON const* req);

private:
  std::shared_ptr<RpcConnectedClient> m_client;
  std::mutex                          m_mutex;
  std::shared_ptr<std::thread>        m_dispatch_thread;
  std::queue<cJSON *>                 m_incoming_queue;
  std::condition_variable             m_cond;
  std::map< std::string, std::shared_ptr<RpcService> > m_services;
  std::string                         m_config_file;
};

#endif
