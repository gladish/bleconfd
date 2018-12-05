//
// Copyright [2018] [jacobgladish@yahoo.com]
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
#include "wpaControl.h"
#include "xLog.h"
#include "jsonRpc.h"
#include "util.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <string>
#include <queue>
#include <pthread.h>
#include <sstream>
#include <vector>

#include <wpa_ctrl.h>
#include <cJSON.h>

static struct wpa_ctrl* wpa_request = nullptr;
static void* wpa_notify_read(void* argp);
static int wpa_shutdown_pipe[2];
static pthread_t wpa_notify_thread;

static cJSON* wpaControl_createResponse(std::string const& s);
static cJSON* wpaControl_createError(int err);
static ResponseSender enqueue_async_message = nullptr;
static int connect_req_id = 0;

int
wpaControl_init(char const* control_socket, ResponseSender const& sender)
{
  if (!control_socket)
    return EINVAL;

  if (!sender)
    return EINVAL;

  enqueue_async_message = sender;

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

  wpa_ctrl_attach(wpa_notify);
  pthread_create(&wpa_notify_thread, nullptr, &wpa_notify_read, wpa_notify);

  return 0;
}

void*
wpa_notify_read(void* argp)
{
  struct wpa_ctrl* wpa_notify = reinterpret_cast<struct wpa_ctrl *>(argp);

  int wpa_fd = wpa_ctrl_get_fd(wpa_notify);
  int shutdown_fd = wpa_shutdown_pipe[0];

  int maxfd = wpa_fd;
  if (shutdown_fd > maxfd)
    maxfd = shutdown_fd;

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

    int ret = select(maxfd, &readfds, nullptr, &errfds, nullptr);
    if (ret > 0)
    {
      if (FD_ISSET(shutdown_fd, &readfds))
      {
        running = false;
      }
      else
      {
        char buff[1024] = {0};
        size_t n = sizeof(buff);

        ret = wpa_ctrl_recv(wpa_notify, buff, &n);
        if (ret < 0) 
          XLOG_ERROR("error reading from WPA socket:%s", strerror(errno));
        else 
          XLOG_INFO("event:%.*s", n, buff);
      }
    }
  }

  wpa_ctrl_close(wpa_notify);
 
  return nullptr;
}

int
wpaControl_command(char const* cmd, std::string& res)
{
  res.reserve(4096);
  res.resize(4069);

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
    XLOG_WARN("failed to submit wpa control request:%d", ret);
    return errno;
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
  if (ret < 0)
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


std::string get_wlan_state()
{
  std::string statusBuffer;
  wpaControl_command("STATUS", statusBuffer);
  std::vector <std::string> lines = split(statusBuffer, "\n");
  for (size_t i = 0; i < lines.size(); i++)
  {
    std::vector <std::string> parts = split(lines[i], "=");
    if (!parts[0].compare("wpa_state"))
    {
      return parts[1];
    }
  }
  return std::string();
}

cJSON*
wpaControl_createAsyncConnectResponse(std::string const& message, std::string const& state)
{
  cJSON* response = cJSON_CreateObject();
  cJSON* temp = cJSON_CreateObject();
  cJSON_AddItemToObject(temp, "jsonrpc", cJSON_CreateString(JSON_RPC_VERSION));
  cJSON_AddItemToObject(temp, "id", cJSON_CreateNumber(connect_req_id));
  cJSON_AddItemToObject(response, "result", temp);
  cJSON_AddItemToObject(response, "state", cJSON_CreateString(state.c_str()));
  cJSON_AddItemToObject(response, "message", cJSON_CreateString(message.c_str()));
  return response;
}

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
      enqueue_async_message(rsp);
      running = false;
    } else if (!state.compare("COMPLETED"))
    {

      cJSON* rsp = wpaControl_createAsyncConnectResponse("connect successful", state);
      std::string statusBuffer;
      wpaControl_command("STATUS", statusBuffer);
      cJSON_AddItemToObject(rsp, "wlanStatus", wpaControl_createResponse(statusBuffer));

      enqueue_async_message(rsp);
      running = false;
    }
    usleep(1000 * 100);  // 200 ms check
  }
}

int
wpaControl_connectToNetwork(cJSON const* req, cJSON** res)
{
  char* s = cJSON_Print(req);
  XLOG_INFO("connect:%s", s);
  free(s);

  cJSON const* params = cJSON_GetObjectItem(req, "params");
  if (!params)
  {
    XLOG_WARN("missing params");
    return -1;
  }

  cJSON* creds = cJSON_GetObjectItem(params, "cred");
  if (!creds)
  {
    XLOG_WARN("params is missing 'creds' object");
    return -1;
  }

  cJSON* disco = cJSON_GetObjectItem(params, "discovery");
  if (!disco)
  {
    XLOG_WARN("params is missing 'discovery' object");
    return -1;
  }

  char const* pass = jsonRpc_getString(creds, "pass", true);
  char const* ssid = jsonRpc_getString(disco, "ssid", true);
  int reqId = jsonRpc_getInt(req, "id", true);

  XLOG_INFO("%s(ssid=%s pwd=%s reqId=%d)", __FUNCTION__, ssid, pass, reqId);

  int networkId = -1;
  int ret = wpaControl_create_network(&networkId);
  if (ret)
  {
    int err = errno;
    XLOG_WARN("failed to create network:%s", strerror(err));
    return err;
  }

  XLOG_INFO("new network created, index = %d", networkId);

  wpaControl_connect_WPA2(networkId, ssid, pass);

//   pthread_t watch_state_t;
//   pthread_create(&watch_state_t, nullptr, &watch_wlan_state, nullptr);

  // here don't return any result
  // connect response will return async

  *res = nullptr;
  return 0;
}

int
wpaControl_getStatus(cJSON const* UNUSED_PARAM(req), cJSON** res)
{
  std::string buff;

  int ret = wpaControl_command("STATUS", buff);
  if (ret)
  {
    *res = wpaControl_createError(ret);
  }
  else
  {
    *res = wpaControl_createResponse(buff);
  }

  return 0;
}

cJSON*
wpaControl_createResponse(std::string const& s)
{
  cJSON* res = cJSON_CreateObject();

  if (!s.empty())
  {
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

  cJSON* temp = nullptr;
  jsonRpc_makeError(&temp, err, "%s", s);

  return temp;
}
