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
#ifndef __GATT_SERVER_H__
#define __GATT_SERVER_H__

#include <memory>
#include <mutex>
#include <queue>

extern "C" 
{
#include <lib/bluetooth.h>
#include <lib/l2cap.h>
#include <lib/uuid.h>
#include <src/shared/att.h>
#include <src/shared/gatt-db.h>
#include <src/shared/gatt-server.h>
}

#include <cJSON.h>

struct gatt_db_attribute;

class GattClient
{
public:
  GattClient(int fd);
  ~GattClient();

  using data_handler = std::function<void (cJSON* json)>;

  void init();
  void run();
  void enqueueForSend(cJSON* json);

  inline void setDataHandler(data_handler h)
    { m_data_handler = h; }

private:
  static void outgoingDataCallback(int fd, uint32_t events, void* argp);
  static void disconnectCallback(int err, void* argp);

private:
  void doSend();

private:
  int                 m_fd;
  bt_att*             m_att;
  gatt_db*            m_db;
  bt_gatt_server*     m_server;
  int                 m_pipe[2];
  uint16_t            m_mtu;
  std::queue<cJSON *> m_outgoing_queue;
  std::mutex          m_mutex;
  data_handler        m_data_handler;
};

class GattServer
{
public:
  GattServer();
  ~GattServer();

  void init();
  std::shared_ptr<GattClient> accept();

private:
  int       m_listen_fd;
  bdaddr_t  m_local_interface;
};

#endif
