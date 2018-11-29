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
#include <thread>
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

  struct DeviceInfoProvider
  {
    std::function< std::string () > get_system_id;
    std::function< std::string () > get_model_number;
    std::function< std::string () > get_serial_number;
    std::function< std::string () > get_firmware_revision;
    std::function< std::string () > get_hardware_revision;
    std::function< std::string () > get_software_revision;
    std::function< std::string () > get_manufacturer_name;
  };

  using data_handler = std::function<void (cJSON* json)>;

  void init(DeviceInfoProvider const& p);
  void run();
  void enqueueForSend(cJSON* json);

  inline void setDataHandler(data_handler h)
    { m_data_handler = h; }

private:
  static void onAsyncMessage(int fd, uint32_t events, void* argp);
  static void disconnectCallback(int err, void* argp);

  static void onInboxWrite(gatt_db_attribute* attr, uint32_t id, uint16_t offset,
    uint8_t const* data, size_t len, uint8_t opcode, bt_att* att, void* argp);

  static void onOutboxRead(gatt_db_attribute* attr, uint32_t id, uint16_t offset,
    uint8_t opcode, bt_att* att, void* argp);

  static void onGapRead(gatt_db_attribute* attr, uint32_t id, uint16_t offset,
    uint8_t opcode, bt_att* att, void* argp);

  static void onGapWrite(gatt_db_attribute* attr, uint32_t id, uint16_t offset,
    uint8_t const* data, size_t len, uint8_t opecode, bt_att* att, void* argp);

  static void onServiceChanged(gatt_db_attribute* attr, uint32_t id, uint16_t offset,
    uint8_t opcode, bt_att* att, void* argp);

  static void onServiceChangedRead(gatt_db_attribute* attr, uint32_t id, uint16_t offset,
    uint8_t opcode, bt_att* att, void* argp);

  static void onServiceChangedWrite(gatt_db_attribute* attr, uint32_t id, uint16_t offset,
    uint8_t const* value, size_t len, uint8_t opcode, bt_att* att, void* argp);

private:
  void processOutgoingMessageQueue();
  void buildGattDatabase();

  void buildGapService();
  void buildGattService();
  void buildDeviceInfoService();
  void addDeviceInfoCharacteristic(gatt_db_attribute* service, uint16_t id,
    std::function<std::string ()> const& read_callback);
  void buildJsonRpcService();

  void onInboxWrite(uint32_t id, uint8_t const* data, uint16_t offset, size_t len);
  void onOutboxRead(uint32_t id, uint16_t offset);

private:
  int                 m_fd;
  bt_att*             m_att;
  gatt_db*            m_db;
  bt_gatt_server*     m_server;
  int                 m_pipe[2];
  uint16_t            m_mtu;
  uint8_t*            m_rpc_buffer;
  uint32_t            m_rpc_buffer_len;
  uint8_t*            m_outbox_buffer;
  uint32_t            m_outbox_buffer_len;
  std::queue<cJSON *> m_outgoing_queue;
  std::mutex          m_mutex;
  data_handler        m_data_handler;
  DeviceInfoProvider  m_dis_provider;
  gatt_db_attribute*  m_inbox;
  gatt_db_attribute*  m_outbox;
  std::thread::id     m_mainloop_thread;
  bool                m_service_change_enabled;
};

class GattServer
{
public:
  GattServer();
  ~GattServer();

  void init();
  std::shared_ptr<GattClient> accept(GattClient::DeviceInfoProvider const& p);

private:
  int       m_listen_fd;
  bdaddr_t  m_local_interface;
};

#endif
