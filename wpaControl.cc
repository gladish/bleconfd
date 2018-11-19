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

#include <wpa_ctrl.h>
#include <cJSON.h>

static struct wpa_ctrl* wpa_request = nullptr;
static void* wpa_notify_read(void* argp);
static int wpa_shutdown_pipe[2];
static pthread_t wpa_notify_thread;

static cJSON* wpaControl_createResponse(std::string const& s);
static cJSON* wpaControl_createError(int err);
static ResponseSender enqueue_async_message = nullptr;

int
wpaControl_init(char const* control_socket, ResponseSender const& sender)
{
  if (!control_socket)
    return EINVAL;

  if (!sender)
    return EINVAL;

  enqueue_async_message = sender;

  std::string         wpa_socket_name = control_socket;
  struct wpa_ctrl*    wpa_notify = nullptr;

  pipe2(wpa_shutdown_pipe, O_CLOEXEC);

  wpa_request = wpa_ctrl_open(wpa_socket_name.c_str());
  if (!wpa_request) 
    return errno;

  wpa_notify = wpa_ctrl_open(wpa_socket_name.c_str());
  if (!wpa_notify) 
  {
    int err = errno;
    if (wpa_request)
      wpa_ctrl_close(wpa_request);
    return err;
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
          printf("error:%s\n", strerror(errno));
        else 
          printf("event:%.*s\n", n, buff);
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
  res.resize(4096);

  size_t n = res.capacity();

  int ret = wpa_ctrl_request(wpa_request, cmd, strlen(cmd), &res[0], &n, nullptr);
  if (ret < 0)
    return errno;

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

int
wpaControl_connectToNetwork(cJSON const*, cJSON**)
{
  // TODO
  return 0;
}

int
wpaControl_getStatus(cJSON const* UNUSED_PARAM(req), cJSON** res)
{
  std::string buff;

  int ret = wpaControl_command("STATUS", buff);
  if (ret)
    *res = wpaControl_createError(ret);
  else
    *res = wpaControl_createResponse(buff);

  return 0;
}

cJSON*
wpaControl_createResponse(std::string const& s)
{
  cJSON* res = cJSON_CreateObject();

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
