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
#include "wifiservice.h"

#include "../defs.h"
#include "../rpclogger.h"
#include "../jsonrpc.h"
#include "../util.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <iostream>
#include <string>
#include <queue>
#include <pthread.h>
#include <sstream>
#include <vector>

#include <wpa_ctrl.h>
#include <cJSON.h>

extern "C"
{
  RpcService*
  WiFiService_Create()
  {
    return new WiFiService();
  }
}

static struct wpa_ctrl* wpa_request = nullptr;
static int wpa_shutdown_pipe[2];
static pthread_t wpa_notify_thread;

static cJSON* wpaControl_createResponse(std::string const& s);
static cJSON* wpaControl_createError(int err);
static void*  wpaControl_readNotificationSocket(void* argp);
static void   wpaControl_reportEvent(char const* buff, int n);
static RpcNotificationFunction responseHandler = nullptr;

static cJSON* wpaControl_parseScanResult(std::string const& line);

static bool
ok(std::string const& s)
{
  return s == "OK";
}

int
wpaControl_init(char const* control_socket, RpcNotificationFunction const& callback)
{
  if (!control_socket)
  {
    XLOG_WARN("NULL control socket path");
    return EINVAL;
  }

  if (!callback)
  {
    XLOG_WARN("NULL response callback handler");
    return EINVAL;
  }

  responseHandler = callback;

  std::string wpa_socket_name = control_socket;
  struct wpa_ctrl* wpa_notify = nullptr;

  int ret = pipe2(wpa_shutdown_pipe, O_CLOEXEC);
  if (ret == -1)
  {
    int err = errno;
    XLOG_ERROR("failed to create shutdown pipe. %s", strerror(err));
    return err;
  }

  wpa_request = wpa_ctrl_open(wpa_socket_name.c_str());
  if (!wpa_request) 
  {
    int err = errno;
    XLOG_ERROR("failed to open:%s. %s", wpa_socket_name.c_str(), strerror(err));
    return err;
  }
  else
  {
    XLOG_INFO("wpa request socket:%s opened for synchronous requests", wpa_socket_name.c_str());
  }

  wpa_notify = wpa_ctrl_open(wpa_socket_name.c_str());
  if (!wpa_notify) 
  {
    int err = errno;
    XLOG_ERROR("failed to open notify socket:%s. %s", wpa_socket_name.c_str(), strerror(err));
    if (wpa_request)
    {
      wpa_ctrl_close(wpa_request);
      wpa_request = nullptr;
    }
    return err;
  }
  else
  {
    XLOG_INFO("wpa request socket:%s opened for notification", wpa_socket_name.c_str());
  }

  ret = wpa_ctrl_attach(wpa_notify);
  if (ret != 0)
  {
    int err = errno;
    XLOG_WARN("failed to attach to wpa interface for notification. %s", strerror(err));
  }

  pthread_create(&wpa_notify_thread, nullptr, &wpaControl_readNotificationSocket, wpa_notify);
  return 0;
}

void
wpaControl_reportEvent(char const* buff, int n)
{
  int i;
  int pass;

  static char const* const kEventWhitelist[] =
  {
    WPA_EVENT_CONNECTED,
    WPA_EVENT_DISCONNECTED,
    NULL
  };

  i = 0;
  pass = 0;

  if (!buff || !n)
  {
    XLOG_INFO("null buffer or zero length string");
    return;
  }

  XLOG_DEBUG("event:%s", buff);

  while (!pass && (kEventWhitelist[i] != NULL))
  {
    // scan past the level <n>
    // each event is prefixed with a log level type number
    // <3>CTRL-EVENT-SCAN-RESULTS
    char const* p  = strchr(buff, '>');

    if (p)
      p++;
    else
      p = buff;

    // XLOG_INFO("%s == %s (%d)", p, kEventWhitelist[i], strlen(kEventWhitelist[i]));

    if (strncmp(kEventWhitelist[i], p, strlen(kEventWhitelist[i])) == 0)
      pass = 1;
    i++;
  }

  if (!pass)
  {
    XLOG_DEBUG("ignoring event");
    return;
  }

  XLOG_INFO("sending event");

  cJSON* e = cJSON_CreateObject();
  cJSON_AddItemToObject(e, "jsonrpc", cJSON_CreateString(kJsonRpcVersion));
  cJSON_AddItemToObject(e, "method", cJSON_CreateString("wpa_event"));
  cJSON_AddItemToObject(e, "params", cJSON_CreateString(buff));
  responseHandler(e);
  cJSON_Delete(e);
}

void*
wpaControl_readNotificationSocket(void* argp)
{
  struct wpa_ctrl* wpa_notify = reinterpret_cast<struct wpa_ctrl *>(argp);

  int wpa_fd = wpa_ctrl_get_fd(wpa_notify);
  int shutdown_fd = wpa_shutdown_pipe[0];

  int maxfd = wpa_fd;
  if (shutdown_fd > maxfd)
    maxfd = shutdown_fd;

  XLOG_INFO("notification thread running");

  bool running = true;
  while (running)
  {
    fd_set readfds;
    fd_set errfds;

    FD_ZERO(&readfds);
    FD_ZERO(&errfds);
    FD_SET(wpa_fd, &readfds);
    FD_SET(shutdown_fd, &readfds);
    FD_SET(wpa_fd, &errfds);

    int ret = select(maxfd + 1, &readfds, nullptr, &errfds, nullptr);
    if (ret > 0)
    {
      if (FD_ISSET(shutdown_fd, &readfds))
      {
        running = false;
      }
      else if (FD_ISSET(wpa_fd, &readfds))
      {
        char buff[1024] = {0};
        size_t n = sizeof(buff);

        ret = wpa_ctrl_recv(wpa_notify, buff, &n);
        if (ret < 0) 
        {
          XLOG_ERROR("error reading from WPA socket:%s", strerror(errno));
        }
        else
        {
          if (n < sizeof(buff))
            buff[n] = '\0';
          else
            buff[sizeof(buff) - 1] = '\0';

          wpaControl_reportEvent(buff, n);
        }
      }
    }
  }

  wpa_ctrl_close(wpa_notify);
  return nullptr;
}

int
wpaControl_command(char const* cmd, std::string& res, int max = 4096)
{
  res.reserve(max);
  res.resize(max);

  size_t n = res.capacity();

  XLOG_INFO("wpa command:%s", cmd);
  if (!wpa_request)
  {
    XLOG_ERROR("request handle is null");
    return -EINVAL;
  }

  int ret = wpa_ctrl_request(wpa_request, cmd, strlen(cmd), &res[0], &n, nullptr);
  if (ret < 0)
  {
    int err = errno;
    XLOG_WARN("failed to submit wpa control request:%s", strerror(err));
    return err;
  }

  res.resize(n);
  return 0;
}

int
wpaControl_shutdown()
{
  std::string s = "shutdown-now";
  write(wpa_shutdown_pipe[1], s.c_str(), s.length());

  void* retval = nullptr;
  pthread_join(wpa_notify_thread, &retval);
  return 0;
}

static int
wpaControl_create_network(int* networkId)
{
  std::string res;

  if (!networkId)
    return EINVAL;

  int ret = wpaControl_command("ADD_NETWORK", res);
  if (ret != 0)
  {
    int err = errno;
    XLOG_ERROR("ADD_NETWORK failed:%s", strerror(err));
    return err;
  }

  *networkId = static_cast<int>(strtol(res.c_str(), NULL, 10));

  return 0;
}

static int
wpaControl_connect_WPA2(int networkId, char const* ssid, char const* wpa_pass)
{
  std::string buff;

  int ret;
  char command_buff[512];

  sprintf(command_buff, "SET_NETWORK %d ssid \"%s\"", networkId, ssid);
  ret = wpaControl_command(command_buff, buff);
  if (ret < 0)
  {
    XLOG_ERROR("wpaControl_connect_WPA2 set ssid failed");
    return errno;
  }

  sprintf(command_buff, "SET_NETWORK %d psk \"%s\"", networkId, wpa_pass);
  ret = wpaControl_command(command_buff, buff);
  if (ret < 0)
  {
    XLOG_ERROR("wpaControl_connect_WPA2 set psk failed");
    return errno;
  }

  XLOG_DEBUG("SET_NETWORK successful %d", networkId);

  sprintf(command_buff, "SELECT_NETWORK %d", networkId);
  ret = wpaControl_command(command_buff, buff);
  if (ret < 0)
  {
    XLOG_ERROR("wpaControl_connect_WPA2 select network failed failed");
    return errno;
  }

  wpaControl_command("SAVE_CONFIG", buff);

  XLOG_DEBUG("SELECT_NETWORK successful %d", networkId);
  return 0;
}


std::string
wpaControl_getState()
{
  std::string res;
  wpaControl_command("STATUS", res);

  std::vector<std::string> lines = split(res, "\n");
  for (size_t i = 0; i < lines.size(); i++)
  {
    std::vector<std::string> parts = split(lines[i], "=");
    if (!parts[0].compare("wpa_state"))
      return parts[1];
  }

  return std::string();
}

#if 0
void*
watch_wlan_state(void* UNUSED_PARAM(argp))
{
  bool running = true;
  int pre_req_id = connect_req_id;
  XLOG_DEBUG("watch_wlan_state create new thread to check state, req_id= %d", pre_req_id);
  usleep(1000 * 1200); // wait some time then start check
  // the state should be from "4WAY_HANDSHAKE" to something else
  while (true)
  {
    if (!running || pre_req_id != connect_req_id) // ignore pre connect request thread
    {
      XLOG_DEBUG("watch_wlan_state thread exit req_id = %d", pre_req_id);
      return nullptr;
    }

    std::string state = get_wlan_state();

    if (!state.compare("SCANNING")
        || !state.compare("DISCONNECTED")
        || !state.compare("INACTIVE")
        || !state.compare("INTERFACE_DISABLED")
        ) // connect failed
    {
      cJSON* rsp = wpaControl_createAsyncConnectResponse("connect failed, maybe password wrong or something else",
                                                         state);
      responseHandler(rsp);
      running = false;
    } else if (!state.compare("COMPLETED"))
    {

      cJSON* rsp = wpaControl_createAsyncConnectResponse("connect successful", state);
      std::string statusBuffer;
      wpaControl_command("STATUS", statusBuffer);
      cJSON_AddItemToObject(rsp, "wlanStatus", wpaControl_createResponse(statusBuffer));

      responseHandler(rsp);
      running = false;
    }
    usleep(1000 * 100);  // 200 ms check
  }
}
#endif


cJSON*
wpaControl_connectToNetwork(cJSON const* req)
{
  char* s = cJSON_Print(req);
  XLOG_INFO("connect:%s", s);
  free(s);

  cJSON const* pass = JsonRpc::search(req, "/params/cred/pass", true);
  cJSON const* ssid = JsonRpc::search(req, "/params/discovery/ssid", true);

  int networkId = -1;
  int ret = wpaControl_create_network(&networkId);
  if (ret)
  {
    int err = errno;
    XLOG_WARN("failed to create network:%s", strerror(err));
    return nullptr;
  }

  XLOG_INFO("new network created, index = %d", networkId);

  wpaControl_connect_WPA2(networkId, ssid->valuestring, pass->valuestring);
  return nullptr;
}


cJSON*
wpaControl_getStatus(cJSON const* UNUSED_PARAM(req))
{
  std::string buff;

  cJSON* res = nullptr;

  int ret = wpaControl_command("STATUS", buff);
  if (ret)
    res = wpaControl_createError(ret);
  else
    res = wpaControl_createResponse(buff);

  return res;
}

cJSON*
wpaControl_createResponse(std::string const& s)
{
  cJSON* res = nullptr;

  if (!s.empty())
  {
    res = cJSON_CreateObject();

    size_t begin = 0;
    while (true)
    {
      size_t end = s.find('\n', begin);
      if (end == std::string::npos)
        break;

      std::string line(s.substr(begin, (end - begin)));
      size_t mid = line.find('=');

      if (mid != std::string::npos)
      {
        std::string name(line.substr(0, mid));
        std::string value(line.substr(mid + 1));

        cJSON_AddItemToObject(res, name.c_str(), cJSON_CreateString(value.c_str()));
      }

      begin = end + 1;
    }
  }

  return res;
}

cJSON*
wpaControl_createError(int err)
{
  char buff[256] = {0};
  char* s = strerror_r(err, buff, sizeof(buff));
  return JsonRpc::makeError(err, s);
}


WiFiService::WiFiService()
  : BasicRpcService("wifi")
{
}

WiFiService::~WiFiService()
{
  wpaControl_shutdown();
}

void
WiFiService::init(cJSON const* conf, RpcNotificationFunction const& callback)
{
  BasicRpcService::init(conf, callback);

  char const* iface = JsonRpc::getString(conf, "/settings/interface", true);
  wpaControl_init(iface, callback);

  registerMethod("get-status", [this](cJSON const* req) -> cJSON* { return this->getStatus(req); });
  registerMethod("connect", [this](cJSON const* req) -> cJSON* { return this->connect(req); });
  registerMethod("scan", [this](cJSON const* req) -> cJSON* { return this->scan(req); });
}

cJSON*
WiFiService::getStatus(cJSON const* req)
{
  return wpaControl_getStatus(req);
}

cJSON*
WiFiService::connect(cJSON const* req)
{
  return wpaControl_connectToNetwork(req);
}

cJSON*
WiFiService::scan(cJSON const* req)
{
  cJSON const* params = cJSON_GetObjectItem(req, "params");

  int reqId = JsonRpc::getInt(req, "id", true);
  char const* band = JsonRpc::getString(params, "band", false);

  std::string buff;

  int ret = wpaControl_command("SCAN", buff);
  if (ret)
  {
    XLOG_WARN("error starting scan:%s", strerror(ret));
  }
  else
  {
    if (!ok(buff))
      XLOG_WARN("error starting scan:%s", buff.c_str());
  }

  cJSON* start = cJSON_CreateObject();
  cJSON_AddStringToObject(start, "status", "start-scan");
  notifyAndDelete(JsonRpc::wrapResponse(0, start, reqId));

  int id = 0;
  while (true)
  {
    char cmd[16];
    snprintf(cmd, sizeof(cmd), "BSS %d", id);

    buff.resize(0);
    ret = wpaControl_command(cmd,  buff, 8192);
    if (!ret)
    {
      cJSON* bss = wpaControl_createResponse(buff);
      if (bss)
        notifyAndDelete(JsonRpc::wrapResponse(0, bss, reqId));
    }

    id++;

    if (buff.size() == 0)
      break;
  }

  cJSON* res = cJSON_CreateObject();
  cJSON_AddStringToObject(res, "status", "scan-done");
  return res;
}
