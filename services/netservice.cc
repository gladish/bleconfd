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
#include "netservice.h"
#include "../rpclogger.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <cJSON.h>

JSONRPC_SERVICE_DEFINE(net, []{return new NetService();});


namespace
{
  char const*
  to_string(sockaddr* s, char* buff, int n)
  {
    void* p = nullptr;
    if (s->sa_family == AF_INET)
    {
      sockaddr_in* sin = reinterpret_cast<sockaddr_in *>(s);
      p = reinterpret_cast< void * >(&sin->sin_addr);
    }
    else if (s->sa_family == AF_INET6)
    {
      sockaddr_in6* sin = reinterpret_cast<sockaddr_in6 *>(s);
      p = reinterpret_cast< void * >(&sin->sin6_addr);
    }

    memset(buff, 0, n);

    char const* str = inet_ntop(s->sa_family, p, buff, n);
    return str;
  }

  cJSON*
  get_device(cJSON* res, char const* devname)
  {
    cJSON* obj = cJSON_GetObjectItem(res, "interfaces");
    if (!obj)
    {
      obj = cJSON_CreateArray();
      cJSON_AddItemToObject(res, "interfaces", obj);
    }

    cJSON* dev = nullptr;
    for (int i = 0, n = cJSON_GetArraySize(obj); i < n; ++i)
    {
      cJSON* item = cJSON_GetArrayItem(obj, i);
      cJSON* name = cJSON_GetObjectItem(item, "dev");
      if (name && (strcmp(name->valuestring, devname) == 0))
      {
        dev = item;
        break;
      }
    }
    if (!dev)
    {
      dev = cJSON_CreateObject();
      cJSON_AddItemToObject(dev, "dev", cJSON_CreateString(devname));
      cJSON_AddItemToArray(obj, dev);
    }

    return dev;
  }
}

NetService::NetService()
  : BasicRpcService("net")
{
}

NetService::~NetService()
{
}

void
NetService::init(cJSON const* conf, RpcNotificationFunction const& callback)
{
  BasicRpcService::init(conf, callback);
  registerMethod("get-interfaces", [this](cJSON const* req) -> cJSON* { return this->getInterfaces(req); });
}

cJSON*
NetService::getInterfaces(cJSON const* UNUSED_PARAM(req))
{

  cJSON* res = nullptr;

  ifaddrs* ifaddr = nullptr;
  int ret = getifaddrs(&ifaddr);
  if (ret == -1)
  {
    XLOG_ERROR("getifaddrs:%d", errno);
    return nullptr;
  }

  res = cJSON_CreateObject();

  for (ifaddrs* i = ifaddr; i != nullptr; i = i->ifa_next)
  {
    int family = i->ifa_addr->sa_family;
    if (family != AF_INET && family != AF_INET6)
      continue;

    char host[INET6_ADDRSTRLEN];

    cJSON* dev = get_device(res, i->ifa_name);
    cJSON* addrs = cJSON_GetObjectItem(dev, "addrs");
    if (!addrs)
    {
      addrs = cJSON_CreateArray();
      cJSON_AddItemToObject(dev, "addrs", addrs);
    }

    cJSON* entry = cJSON_CreateObject();
    to_string(i->ifa_addr, host, sizeof(host));
    cJSON_AddStringToObject(entry, (family == AF_INET ? "inet" : "inet6"), host);

    if (i->ifa_netmask && family == AF_INET)
    {
      cJSON_AddStringToObject(entry, "mask", to_string(i->ifa_netmask, host, sizeof(host)));
      if (strcmp(i->ifa_name, "lo") != 0)
        cJSON_AddStringToObject(entry, "broadcast", to_string(i->ifa_broadaddr, host, sizeof(host)));
    }
    else if (family == AF_INET6)
    {
      sockaddr_in6* sin6 = reinterpret_cast<sockaddr_in6 *>(i->ifa_addr);
      if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr))
        cJSON_AddStringToObject(entry, "scope", "link");
      else if (IN6_IS_ADDR_LOOPBACK(&sin6->sin6_addr))
        cJSON_AddStringToObject(entry, "scope", "host");
      else if (IN6_IS_ADDR_SITELOCAL(&sin6->sin6_addr))
        cJSON_AddStringToObject(entry, "scope", "site");
      else
        cJSON_AddStringToObject(entry, "scope", "global");
    }

    cJSON_AddItemToArray(addrs, entry);
  }

  if (ifaddr)
    freeifaddrs(ifaddr);

  return res;
}
