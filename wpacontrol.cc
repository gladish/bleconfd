
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

#include <build_config.h>
#include <wpa_ctrl.h>
#include <cJSON.h>

#define UNUSED_PARAM(X) UNUSED_ ## X __attribute__((__unused__))

// public interface to managing/controlling wpa_supplicant

int wpa_control_init();
int wpa_control_shutdown();
int wpa_control_command(cJSON const* req, cJSON** res);

cJSON* make_request(char const* cmd)
{
  cJSON* doc = cJSON_CreateObject();
  cJSON_AddItemToObject(doc, "cmd", cJSON_CreateString(cmd));
  return doc;
}

static struct wpa_ctrl* wpa_request = nullptr;
static void* wpa_notify_read(void* argp);
static int wpa_shutdown_pipe[2];
static pthread_t wpa_notify_thread;

int main(int UNUSED_PARAM(argc), char** UNUSED_PARAM(argv))
{
  wpa_control_init();
  // yes this leaks the request
  wpa_control_command(make_request("STATUS"), nullptr);
  wpa_control_command(make_request("STATUS"), nullptr);
  wpa_control_shutdown();
  return 0;
}

int
wpa_control_init()
{
  std::string         wpa_socket_name = "/var/run/wpa_supplicant/wlan0";
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
wpa_control_command(cJSON const* req, cJSON** res)
{
  size_t            n;
  std::vector<char> buff;
  char const*       cmd;

  buff.reserve(4096);
  buff.resize(4096);
  n = buff.capacity();

  cJSON* temp = cJSON_GetObjectItem(req, "cmd");
  cmd = temp->valuestring;

  int ret = wpa_ctrl_request(wpa_request, cmd, strlen(cmd), buff.data(), &n, nullptr);
  if (ret < 0)
    return errno;

  printf("%.*s\n", n, buff.data());
  return 0;
}

int
wpa_control_shutdown()
{
  std::string s = "shutdown-now";
  write(wpa_shutdown_pipe[1], s.c_str(), s.length());

  void* retval = nullptr;
  pthread_join(wpa_notify_thread, &retval);
  return 0;
}
