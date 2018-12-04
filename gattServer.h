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

#include <list>
#include <memory>
#include <thread>
#include <vector>
#include <sstream>

#include "memory_stream.h"

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
  void enqueueForSend(cJSON const* json);

  inline void setDataHandler(data_handler h)
    { m_data_handler = h; }

  void onDataChannelOut(gatt_db_attribute* attr, uint32_t id, uint16_t offset,
    uint8_t opcode, bt_att* att);

  void onDataChannelIn(gatt_db_attribute* attr, uint32_t id, uint16_t offset,
    uint8_t const* data, size_t len, uint8_t opcode, bt_att* att);

  void onTimeout();

  void onEPollRead(gatt_db_attribute* attr, uint32_t id, uint16_t offset,
    uint8_t opcode, bt_att* att);

  void onClientDisconnected(int err);

  void onGapRead(gatt_db_attribute* attr, uint32_t id, uint16_t offset,
    uint8_t opcode, bt_att* att);

  void onGapWrite(gatt_db_attribute* attr, uint32_t id, uint16_t offset,
    uint8_t const* data, size_t len, uint8_t opcode, bt_att* att);

  void onServiceChanged(gatt_db_attribute* attr, uint32_t id, uint16_t offset,
    uint8_t opcode, bt_att* att);

  void onServiceChangedRead(gatt_db_attribute* attr, uint32_t id, uint16_t offset,
    uint8_t opcode, bt_att* att);

  void onServiceChangedWrite(gatt_db_attribute* attr, uint32_t id, uint16_t offset,
    uint8_t const* value, size_t len, uint8_t opcode, bt_att* att);

  void onGapExtendedPropertiesRead(gatt_db_attribute *attrib, uint32_t id,
    uint16_t offset, uint8_t opcode, bt_att* att);

private:
  void buildGattDatabase();

  void buildGapService();
  void buildGattService();
  void buildDeviceInfoService();
  void addDeviceInfoCharacteristic(gatt_db_attribute* service, uint16_t id,
    std::function<std::string ()> const& read_callback);
  void buildJsonRpcService();

private:
  int                 m_fd;
  bt_att*             m_att;
  gatt_db*            m_db;
  bt_gatt_server*     m_server;
  uint16_t            m_mtu;
  memory_stream       m_outgoing_queue;
  std::vector<char>   m_incoming_buff;
  data_handler        m_data_handler;
  DeviceInfoProvider  m_dis_provider;
  gatt_db_attribute*  m_data_channel;
  gatt_db_attribute*  m_blepoll;
  uint16_t            m_notify_handle;
  bool                m_service_change_enabled;
  int                 m_timeout_id;
  std::thread::id     m_mainloop_thread;
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
