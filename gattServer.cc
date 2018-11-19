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

#include <assert.h>
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
  uint16_t const kUuidDeviceInfoService   {0x180a};
  static const uint16_t kUuidGap          {0x1800};
  static const uint16_t kUuidGatt         {0x1801};

  uint16_t const kUuidSystemId            {0x2a23};
  uint16_t const kUuidModelNumber         {0x2a24};
  uint16_t const kUuidSerialNumber        {0x2a25};
  uint16_t const kUuidFirmwareRevision    {0x2a26};
  uint16_t const kUuidHardwareRevision    {0x2a27};
  uint16_t const kUuidSoftwareRevision    {0x2a28};
  uint16_t const kUuidManufacturerName    {0x2a29};

  std::string const kUuidRpcService       {"503553ca-eb90-11e8-ac5b-bb7e434023e8"};
  std::string const kUuidRpcInbox         {"510c87c8-eb90-11e8-b3dc-17292c2ecc2d"};
  std::string const kUuidRpcOutbox        {"5140f882-eb90-11e8-a835-13d2bd922d3f"};

  std::string const kOutgoingDataMessage  {"outgoing-data"};

  void DIS_writeCallback(gatt_db_attribute* UNUSED_PARAM(attr), int err, void* UNUSED_PARAM(argp))
  {
    if (err)
    {
      XLOG_WARN("error writing to DIS service in GATT db. %d", err);
    }
  }

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
GattServer::accept(GattClient::DeviceInfoProvider const& p)
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
  clnt->init(p);

  return clnt;
}

void
GattClient::init(DeviceInfoProvider const& p)
{
  m_dis_provider = p;
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

  mainloop_add_fd(m_pipe[0], EPOLLIN, &GattClient::onAsyncMessage, this, nullptr);
  buildGattDatabase();
}

void
GattClient::buildGattDatabase()
{
  buildGapService();
  buildGattService();
  buildDeviceInfoService();
  buildJsonRpcService();
}

void
GattClient::buildJsonRpcService()
{
  bt_uuid_t uuid;

  bt_string_to_uuid(&uuid, kUuidRpcService.c_str());
  gatt_db_attribute* service = gatt_db_add_service(m_db, &uuid, true, 2);
  // gatt_db_attribute_get_handle(service); do we need this handle?

  // inbox
  bt_string_to_uuid(&uuid, kUuidRpcInbox.c_str());
  m_inbox = gatt_db_service_add_characteristic(
    service,
    &uuid,
    BT_ATT_PERM_READ | BT_ATT_PERM_WRITE,
    BT_GATT_CHRC_PROP_WRITE,
    nullptr,
    &GattClient::onInboxWrite, // remote client is writing to inbox
    this);

  // outbox
  bt_string_to_uuid(&uuid, kUuidRpcOutbox.c_str());
  m_outbox = gatt_db_service_add_characteristic(
    service,
    &uuid,
    BT_ATT_PERM_READ | BT_ATT_PERM_WRITE,
    BT_GATT_CHRC_PROP_READ | BT_GATT_CHRC_PROP_NOTIFY,
    &GattClient::onOutboxRead, // remote client is reading from outbox
    nullptr,
    this);

  gatt_db_service_set_active(service, true);
}

void
GattClient::onInboxWrite(gatt_db_attribute* UNUSED_PARAM(attr), uint32_t id,
  uint16_t offset, uint8_t const* data, size_t len, uint8_t UNUSED_PARAM(opcode),
  bt_att* UNUSED_PARAM(att), void* argp)
{
  GattClient* clnt = reinterpret_cast<GattClient *>(argp);
  clnt->onInboxWrite(id, data, offset, len);
}

void
GattClient::onInboxWrite(uint32_t id, uint8_t const* data, uint16_t offset, size_t len)
{
  // TODO client is writing data into inbox
}

void
GattClient::onOutboxRead(gatt_db_attribute* UNUSED_PARAM(attr), uint32_t id,
  uint16_t offset, uint8_t UNUSED_PARAM(opcode), bt_att* UNUSED_PARAM(att), void* argp)
{
  GattClient* clnt = reinterpret_cast<GattClient *>(argp);
  clnt->onOutboxRead(id, offset);
}

void
GattClient::onOutboxRead(uint32_t id, uint16_t offset)
{
  // TODO: client is reading from inbox
}

void
GattClient::buildGapService()
{
  bt_uuid_t uuid;
  bt_uuid16_create(&uuid, kUuidGap);

  gatt_db_attribute* service = gatt_db_add_service(m_db, &uuid, true, 6);

  // device name
  bt_uuid16_create(&uuid, GATT_CHARAC_DEVICE_NAME);
  gatt_db_service_add_characteristic(service, &uuid, BT_ATT_PERM_READ | BT_ATT_PERM_WRITE,
    BT_GATT_CHRC_PROP_READ | BT_GATT_CHRC_PROP_EXT_PROP,
    &GattClient::onGapRead, &GattClient::onGapWrite, this);

  bt_uuid16_create(&uuid, GATT_CHARAC_EXT_PROPER_UUID);
  gatt_db_service_add_descriptor(service, &uuid, BT_ATT_PERM_READ,
    nullptr, nullptr, this);

  // appearance
  bt_uuid16_create(&uuid, GATT_CHARAC_APPEARANCE);
  gatt_db_attribute* attr = gatt_db_service_add_characteristic(service, &uuid, BT_ATT_PERM_READ,
    BT_GATT_CHRC_PROP_READ, nullptr, nullptr, this);

  uint16_t appearance {0};
  bt_put_le16(128, &appearance);
  gatt_db_attribute_write(attr, 0, reinterpret_cast<uint8_t const *>(&appearance),
      sizeof(appearance), BT_ATT_OP_WRITE_REQ, nullptr, &DIS_writeCallback, nullptr);

  gatt_db_service_set_active(service, true);
}

void
GattClient::onGapRead(gatt_db_attribute* attr, uint32_t id, uint16_t offset,
    uint8_t opcode, bt_att* att, void* argp)
{
  GattClient* clnt = reinterpret_cast<GattClient *>(argp);
  // TODO:
}

void
GattClient::onGapWrite(gatt_db_attribute* attr, uint32_t id, uint16_t offset,
    uint8_t const* data, size_t len, uint8_t opecode, bt_att* att, void* argp)
{
  GattClient* clnt = reinterpret_cast<GattClient *>(argp);
  // TODO:
}

void
GattClient::onServiceChanged(gatt_db_attribute* attr, uint32_t id, uint16_t UNUSED_PARAM(offset),
    uint8_t UNUSED_PARAM(opcode), bt_att* UNUSED_PARAM(att), void* UNUSED_PARAM(argp))
{
  XLOG_INFO("onServiceChanged");
  gatt_db_attribute_read_result(attr, id, 0, nullptr, 0);
}

void
GattClient::buildGattService()
{
  bt_uuid_t uuid;
  bt_uuid16_create(&uuid, kUuidGatt);

  gatt_db_attribute* service = gatt_db_add_service(m_db, &uuid, true, 4);

  bt_uuid16_create(&uuid, GATT_CHARAC_SERVICE_CHANGED);
  gatt_db_service_add_characteristic(service, &uuid, BT_ATT_PERM_READ,
      BT_GATT_CHRC_PROP_READ | BT_GATT_CHRC_PROP_INDICATE,
      GattClient::onServiceChanged, nullptr, this);

  bt_uuid16_create(&uuid, GATT_CLIENT_CHARAC_CFG_UUID);
  gatt_db_service_add_descriptor(service, &uuid, BT_ATT_PERM_READ | BT_ATT_PERM_WRITE,
      GattClient::onServiceChangedRead, GattClient::onServiceChangedWrite, this);

  gatt_db_service_set_active(service, true);
}

void
GattClient::onServiceChangedRead(gatt_db_attribute* attr, uint32_t id, uint16_t UNUSED_PARAM(offset),
  uint8_t UNUSED_PARAM(opcode), bt_att* UNUSED_PARAM(att), void* argp)
{
  GattClient* clnt = reinterpret_cast<GattClient *>(argp);

  uint8_t value[2] {0x00, 0x00};
  if (clnt->m_service_change_enabled)
    value[0] = 0x02;
  gatt_db_attribute_read_result(attr, id, 0, value, sizeof(value));
}

void
GattClient::onServiceChangedWrite(gatt_db_attribute* attr, uint32_t id, uint16_t offset,
    uint8_t const* value, size_t len,
    uint8_t UNUSED_PARAM(opcode), bt_att* UNUSED_PARAM(att), void* argp)
{
  GattClient* clnt = reinterpret_cast<GattClient *>(argp);

  uint8_t ecode = 0;
  if (!value || (len != 2))
    ecode = BT_ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LEN;

  if (!ecode && offset)
    ecode = BT_ATT_ERROR_INVALID_OFFSET;

  if (!ecode)
  {
    if (value[0] == 0x00)
      clnt->m_service_change_enabled = false;
    else if (value[0] == 0x02)
      clnt->m_service_change_enabled = true;
    else
      ecode = 0x80;
  }

  gatt_db_attribute_write_result(attr, id, ecode);
}

void
GattClient::addDeviceInfoCharacteristic(
  gatt_db_attribute* service,
  uint16_t           id,
  std::function< std::string () > const& read_callback)
{
  bt_uuid_t uuid;
  bt_uuid16_create(&uuid, id);

  gatt_db_attribute* attr = gatt_db_service_add_characteristic(service, &uuid, BT_ATT_PERM_READ,
    BT_GATT_CHRC_PROP_READ, nullptr, nullptr, this);
  if (!attr)
  {
    // TODO
  }

  std::string value;
  if (read_callback)
    value = read_callback();

  uint8_t const* p = reinterpret_cast<uint8_t const *>(value.c_str());

  // I'm not sure whether i like this or just having callbacks setup for reads
  gatt_db_attribute_write(attr, 0, p, value.length(), BT_ATT_OP_WRITE_REQ, nullptr,
      &DIS_writeCallback, nullptr);
}

void
GattClient::buildDeviceInfoService()
{
  bt_uuid_t uuid;
  bt_uuid16_create(&uuid, kUuidDeviceInfoService);

  gatt_db_attribute* service = gatt_db_add_service(m_db, &uuid, true, 14); // what's the 14?

  addDeviceInfoCharacteristic(service, kUuidSystemId, m_dis_provider.get_system_id);
  addDeviceInfoCharacteristic(service, kUuidModelNumber, m_dis_provider.get_model_number);
  addDeviceInfoCharacteristic(service, kUuidSerialNumber, m_dis_provider.get_serial_number);
  addDeviceInfoCharacteristic(service, kUuidFirmwareRevision, m_dis_provider.get_firmware_revision);
  addDeviceInfoCharacteristic(service, kUuidHardwareRevision, m_dis_provider.get_hardware_revision);
  addDeviceInfoCharacteristic(service, kUuidSoftwareRevision, m_dis_provider.get_software_revision);
  addDeviceInfoCharacteristic(service, kUuidManufacturerName, m_dis_provider.get_manufacturer_name);

  gatt_db_service_set_active(service, true);
}

void
GattClient::onAsyncMessage(int fd, uint32_t UNUSED_PARAM(events), void* argp)
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
    clnt->processOutgoingMessageQueue();
  }
  else
  {
    XLOG_WARN("unknown async message:%s", buff);
  }
}

void
GattClient::run()
{
  m_mainloop_thread = std::this_thread::get_id();

  // Is there any way to use glib mainloop? we're using the bluez
  // built-in event loop
  mainloop_run();
}

void
GattClient::processOutgoingMessageQueue()
{
  // This should be on the mainloop_ run() thread
  assert( std::this_thread::get_id() == m_mainloop_thread );

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
        // them all as notify to the outbox
      }
      cJSON_Delete(next_outgoing_message);
    }
  }
}

GattClient::GattClient(int fd)
  : m_fd(fd)
  , m_service_change_enabled(false)
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
