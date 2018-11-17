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
#include "gattServer.h"
#include "xLog.h"

#include <exception>
#include <sstream>
#include <vector>

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <cJSON.h>

// from bluez
extern "C" 
{
#include <src/shared/mainloop.h>
}

namespace
{
  std::string const kOutgoingDataMessage { "outgoing-data" };

  void throw_errno(int err, char const* fmt, ...)
    __attribute__ ((format (printf, 2, 3)));

  void ATT_debugCallback(char const* str, void* UNUSED_PARAM(argp))
  {
    if (!str)
      XLOG_DEBUG("ATT: debug callback with no message");
    else
      XLOG_DEBUG("ATT: %s", str);
  }

  void GATT_debugCallback(char const* str, void* UNUSED_PARAM(argp))
  {
    if (!str)
      XLOG_DEBUG("GATT: debug callback with no message");
    else
      XLOG_DEBUG("GATT: %s", str);
  }

  void throw_errno(int e, char const* fmt, ...)
  {
    char buff[256] = {0};

    va_list args;
    va_start(args, fmt);
    vsnprintf(buff, sizeof(buff), fmt, args);
    buff[sizeof(buff) - 1] = '\0';
    va_end(args);

    char err[256] = {0};
    char* p = strerror_r(e, err, sizeof(err));

    std::stringstream out;
    if (strlen(buff) > 0)
    {
      out << buff;
      out << ". ";
    }
    if (p && strlen(p) > 0)
      out << p;

    throw std::runtime_error(out.str());
  }
}

GattServer::GattServer()
  : m_listen_fd(-1)
{
  memset(&m_local_interface, 0, sizeof(m_local_interface));
}

GattServer::~GattServer()
{
  if (m_listen_fd != -1)
    close(m_listen_fd);
}

void
GattServer::init()
{
  m_listen_fd = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
  if (m_listen_fd < 0)
    throw_errno(errno, "failed to create bluetooth socket");

  bdaddr_t src_addr = {0}; // BDADDR_ANY

  sockaddr_l2 srcaddr;
  memset(&srcaddr, 0, sizeof(srcaddr));
  srcaddr.l2_family = AF_BLUETOOTH;
  srcaddr.l2_cid = htobs(4);
  srcaddr.l2_bdaddr_type = BDADDR_LE_PUBLIC;
  bacpy(&srcaddr.l2_bdaddr, &src_addr);

  int ret = bind(m_listen_fd, reinterpret_cast<sockaddr *>(&srcaddr), sizeof(srcaddr));
  if (ret < 0)
    throw_errno(errno, "failed to bind bluetooth socket");

  bt_security btsec = {0, 0};
  btsec.level = BT_SECURITY_LOW;
  ret = setsockopt(m_listen_fd, SOL_BLUETOOTH, BT_SECURITY, &btsec, sizeof(btsec));
  if (ret < 0)
    throw_errno(errno, "failed to set security on bluetooth socket");

  ret = listen(m_listen_fd, 2);
  if (ret < 0)
    throw_errno(errno, "failed to listen on bluetooth socket");
}

std::shared_ptr<GattClient>
GattServer::accept()
{
  mainloop_init();

  sockaddr_l2 peer_addr;
  memset(&peer_addr, 0, sizeof(peer_addr));

  socklen_t n = sizeof(peer_addr);
  int soc = ::accept(m_listen_fd, reinterpret_cast<sockaddr *>(&peer_addr), &n);
  if (soc < 0)
    throw_errno(errno, "failed to accept incoming connection on bluetooth socket");

  char remote_address[64] = {0};
  ba2str(&peer_addr.l2_bdaddr, remote_address);
  XLOG_INFO("accepting remote connection from:%s", remote_address);

  std::shared_ptr<GattClient> clnt(new GattClient(soc));
  clnt->init();

  return clnt;
}

void
GattClient::init()
{
  m_att = bt_att_new(m_fd, 0);
  bt_att_set_close_on_unref(m_att, true);
  bt_att_register_disconnect(m_att, &GattClient::disconnectCallback, this, nullptr);
  m_db = gatt_db_new();
  m_server = bt_gatt_server_new(m_db, m_att, m_mtu);

  if (true)
  {
    bt_att_set_debug(m_att, ATT_debugCallback, this, nullptr);
    bt_gatt_server_set_debug(m_server, GATT_debugCallback, this, nullptr);
  }

  int ret = pipe2(m_pipe, O_CLOEXEC);
  if (ret < 0)
    throw_errno(errno, "failed to create notification pipe");

  mainloop_add_fd(m_pipe[0], EPOLLIN, &GattClient::outgoingDataCallback, this, nullptr);
}

void
GattClient::outgoingDataCallback(int fd, uint32_t UNUSED_PARAM(events), void* argp)
{
  GattClient* clnt = reinterpret_cast<GattClient *>(argp);

  char buff[64] = {0};

  int ret = read(fd, buff, sizeof(buff));
  if (ret < 0)
  {
    XLOG_ERROR("error reading from pipe:%s", strerror(errno));
    return;
  }

  if (strcmp(buff, kOutgoingDataMessage.c_str()) == 0)
  {
    clnt->doSend();
  }
  else
  {
    XLOG_WARN("unknown async message:%s", buff);
  }
}

void
GattClient::run()
{
  // Is there any way to use glib mainloop? we're using the bluez
  // built-in event loop
  mainloop_run();
}

void
GattClient::doSend()
{
  cJSON* next_outgoing_message = nullptr;

  while (true)
  {
    {
      std::lock_guard<std::mutex> guard(m_mutex);
      if (m_outgoing_queue.empty())
        return;

      next_outgoing_message = m_outgoing_queue.front();
      m_outgoing_queue.pop();
    }

    if (next_outgoing_message)
    {
      char* payload = cJSON_PrintUnformatted(next_outgoing_message);
      if (!payload)
      {
        // TODO: split message using a function of the mtu. @see m_mtu. There's
        // also a way to query the att layer for the current mtu size. Account
        // for the 4-byte header, split payload into chunks, and then send
        // them all
      }
      cJSON_Delete(next_outgoing_message);
    }
  }
}

GattClient::GattClient(int fd)
  : m_fd(fd)
{
}


GattClient::~GattClient()
{
  if (m_fd != -1)
    close(m_fd);

  if (m_server)
    bt_gatt_server_unref(m_server);

  if (m_db)
    gatt_db_unref(m_db);
}

void
GattClient::enqueueForSend(cJSON* json)
{
  {
    std::lock_guard<std::mutex> guard(m_mutex);
    m_outgoing_queue.push(json);
  }

  // signal mainloop_run() thread of pending outgoing message
  int ret = write(m_pipe[1], kOutgoingDataMessage.c_str(), kOutgoingDataMessage.size());
  if (ret < 0)
  {
    XLOG_WARN("write:%s", strerror(errno));
  }
}

void
GattClient::disconnectCallback(int err, void* UNUSED_PARAM(argp))
{
  // TODO: we should stash the remote client as a member of the
  // GattClient so we can print out mac addres of client that
  // just disconnected
  XLOG_INFO("disconnect:%d", err);
  mainloop_quit();
}
