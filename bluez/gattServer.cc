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
#include "gattServer.h"
#include "beacon.h"
#include "../defs.h"
#include "../rpclogger.h"
#include "../util.h"

#include <exception>
#include <fstream>
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
  char ServerName[64]                     {"TheUnknownServer"};
  char const      kRecordDelimiter        {30};
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
  std::string const kUuidRpcEPoll         {"5140f882-eb90-11e8-a835-13d2bd922d3f"};

  std::string
  PROC_getVariable(char const* file, char const* field)
  {
    char buff[256];
    FILE* f = fopen(file, "r");
    while (fgets(buff, sizeof(buff) - 1, f) != nullptr)
    {
      if (strncmp(buff, field, strlen(field)) == 0)
      {
        char* p = strchr(buff, ':');
        if (p)
        {
          p++;
          while (p && isspace(*p))
            p++;
        }
        if (p)
        {
          size_t len = strlen(p);
          if (len > 0)
            p[len -1] = '\0';
          return std::string(p);
        }
      }
    }
    return std::string();
  }

  std::vector<std::string> 
  getDeviceInfoFromDB(std::string const& rCode)
  {
    std::ifstream mf("devices_db");
    std::string line;

    while (std::getline(mf, line))
    {
      std::vector<std::string> t = split(line, ",");
      if (!t[0].compare(rCode))
        return t;
    }
    
    return std::vector<std::string>();
  }

  std::string
  getContentFromFile(char const* file)
  {
    std::ifstream mf(file);
    std::string system_id;
    if (std::getline(mf, system_id)) 
    {
      return system_id;
    }
    else 
    {
      return std::string("unkown");
    }
  }
  
  std::string
  DIS_getSystemId()
  {
    // get system id from machine id
    return getContentFromFile("/etc/machine-id");
  }

  std::string DIS_getModelNumber()
  {
    return getContentFromFile("/proc/device-tree/model");
  }

  std::string DIS_getSerialNumber()
  {
    return PROC_getVariable("/proc/cpuinfo", "Serial");
  }

  std::string DIS_getFirmwareRevision()
  {
    // get version from command "uname -a"
    std::string full = runCommand("uname -a");
    size_t index = full.find(" SMP");
    if (index != std::string::npos)
    {
      return full.substr(0, index);
    }
    return full;
  }

  std::string DIS_getHardwareRevision()
  {
    return PROC_getVariable("/proc/cpuinfo", "Revision");
  }

  std::string DIS_getSoftwareRevision()
  {
    return std::string(BLECONFD_VERSION);
  }

  std::string DIS_getManufacturerName()
  {
    std::string rCode = DIS_getHardwareRevision();
    std::vector<std::string> deviceInfo = getDeviceInfoFromDB(rCode);
    
    if( deviceInfo.size() > 0 )
    {
      return deviceInfo[4];
    }
    return std::string("unkown");
  }

  /**
   * dump Provider lines
   **/
  void DIS_dumpProvider()
  {
    XLOG_DEBUG("DIS_getSystemId         = %s", DIS_getSystemId().c_str());
    XLOG_DEBUG("DIS_getModelNumber      = %s", DIS_getModelNumber().c_str());
    XLOG_DEBUG("DIS_getSerialNumber     = %s", DIS_getSerialNumber().c_str());
    XLOG_DEBUG("DIS_getFirmwareRevision = %s", DIS_getFirmwareRevision().c_str());
    XLOG_DEBUG("DIS_getHardwareRevision = %s", DIS_getHardwareRevision().c_str());
    XLOG_DEBUG("DIS_getSoftwareRevision = %s", DIS_getSoftwareRevision().c_str());
    XLOG_DEBUG("DIS_getManufacturerName = %s", DIS_getManufacturerName().c_str());
  }

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

    std::string message(out.str());
    XLOG_ERROR("exception:%s", message.c_str());
    throw std::runtime_error(message);
  }

  void GattClient_onGapRead(gatt_db_attribute* attr, uint32_t id, uint16_t offset,
    uint8_t opcode, bt_att* att, void* argp)
  {
    GattClient* clnt = reinterpret_cast<GattClient *>(argp);
    clnt->onGapRead(attr, id, offset, opcode, att);
  }

  void GattClient_onGapWrite(gatt_db_attribute* attr, uint32_t id, uint16_t offset,
    uint8_t const* data, size_t len, uint8_t opcode, bt_att* att, void* argp)
  {
    GattClient* clnt = reinterpret_cast<GattClient *>(argp);
    clnt->onGapWrite(attr, id, offset, data, len, opcode, att);
  }

  void GattClient_onServiceChanged(gatt_db_attribute* attr, uint32_t id, uint16_t offset,
    uint8_t opcode, bt_att* att, void* argp)
  {
    GattClient* clnt = reinterpret_cast<GattClient *>(argp);
    clnt->onServiceChanged(attr, id, offset, opcode, att);
  }

  void GattClient_onServiceChangedRead(gatt_db_attribute* attr, uint32_t id, uint16_t offset,
    uint8_t opcode, bt_att* att, void* argp)
  {
    GattClient* clnt = reinterpret_cast<GattClient *>(argp);
    clnt->onServiceChangedRead(attr, id, offset, opcode, att);
  }

  void GattClient_onServiceChangedWrite(gatt_db_attribute* attr, uint32_t id, uint16_t offset,
    uint8_t const* value, size_t len, uint8_t opcode, bt_att* att, void* argp)
  {
    GattClient* clnt = reinterpret_cast<GattClient *>(argp);
    clnt->onServiceChangedWrite(attr, id, offset, value, len, opcode, att);
  }

  void GattClient_onGapExtendedPropertiesRead(gatt_db_attribute *attr, uint32_t id,
    uint16_t offset, uint8_t opcode, bt_att* att, void* argp)
  {
    GattClient* clnt = reinterpret_cast<GattClient *>(argp);
    clnt->onGapExtendedPropertiesRead(attr, id, offset, opcode, att);
  }

  void GattClient_onClientDisconnected(int err, void* argp)
  {
    GattClient* clnt = reinterpret_cast<GattClient *>(argp);
    clnt->onClientDisconnected(err);
  }

  void GattClient_onEPollRead(gatt_db_attribute* attr, uint32_t id, uint16_t offset,
    uint8_t opcode, bt_att* att, void* argp)
  {
    GattClient* clnt = reinterpret_cast<GattClient *>(argp);
    clnt->onEPollRead(attr, id, offset, opcode, att);
  }

  void GattClient_onDataChannelIn(gatt_db_attribute* attr, uint32_t id, uint16_t offset,
    uint8_t const* data, size_t len, uint8_t opcode, bt_att* att, void* argp)
  {
    GattClient* clnt = reinterpret_cast<GattClient *>(argp);
    clnt->onDataChannelIn(attr, id, offset, data, len, opcode, att);
  }

  void GattClient_onDataChannelOut(gatt_db_attribute* attr, uint32_t id, uint16_t offset,
    uint8_t opcode, bt_att* att, void* argp)
  {
    GattClient* clnt = reinterpret_cast<GattClient *>(argp);
    clnt->onDataChannelOut(attr, id, offset, opcode, att);
  }

  void GattClient_onTimeout(int UNUSED_PARAM(fd), void* argp)
  {
    GattClient* clnt = reinterpret_cast<GattClient *>(argp);
    clnt->onTimeout();
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
GattServer::init(std::string const& name, std::string const& uuid)
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

  startBeacon(name);
}

std::shared_ptr<RpcConnectedClient>
GattServer::accept()
{
  mainloop_init();

  sockaddr_l2 peer_addr;
  memset(&peer_addr, 0, sizeof(peer_addr));

  XLOG_INFO("waiting for incoming BLE connections");

  socklen_t n = sizeof(peer_addr);
  int soc = ::accept(m_listen_fd, reinterpret_cast<sockaddr *>(&peer_addr), &n);
  if (soc < 0)
    throw_errno(errno, "failed to accept incoming connection on bluetooth socket");

  char remote_address[64] = {0};
  ba2str(&peer_addr.l2_bdaddr, remote_address);
  XLOG_INFO("accepted remote connection from:%s", remote_address);

  auto clnt = std::shared_ptr<GattClient>(new GattClient(soc));
  clnt->init();
  return clnt;
}

void
GattClient::init()
{
  m_att = bt_att_new(m_fd, 0);
  if (!m_att)
  {
    XLOG_ERROR("failed to create new att:%d", errno);
  }

  bt_att_set_close_on_unref(m_att, true);
  bt_att_register_disconnect(m_att, &GattClient_onClientDisconnected, this, nullptr);
  m_db = gatt_db_new();
  if (!m_db)
  {
    XLOG_ERROR("failed to create gatt database");
  }

  m_server = bt_gatt_server_new(m_db, m_att, m_mtu);
  if (!m_server)
  {
    XLOG_ERROR("failed to create gatt server");
  }

  if (true)
  {
    bt_att_set_debug(m_att, ATT_debugCallback, this, nullptr);
    bt_gatt_server_set_debug(m_server, GATT_debugCallback, this, nullptr);
  }

  m_timeout_id = mainloop_add_timeout(1000, &GattClient_onTimeout, this, nullptr);
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
  gatt_db_attribute* service = gatt_db_add_service(m_db, &uuid, true, 25);
  if (!service)
  {
    XLOG_CRITICAL("failed to add rpc service to gatt db");
  }

  // data channel
  bt_string_to_uuid(&uuid, kUuidRpcInbox.c_str());
  m_data_channel = gatt_db_service_add_characteristic(
    service,
    &uuid,
    BT_ATT_PERM_READ | BT_ATT_PERM_WRITE,
    BT_GATT_CHRC_PROP_READ | BT_GATT_CHRC_PROP_WRITE,
    &GattClient_onDataChannelOut,
    &GattClient_onDataChannelIn,
    this);

  if (!m_data_channel)
  {
    XLOG_CRITICAL("failed to create inbox characteristic");
  }

  // blepoll
  bt_string_to_uuid(&uuid, kUuidRpcEPoll.c_str());
  m_blepoll = gatt_db_service_add_characteristic(
    service,
    &uuid,
    BT_ATT_PERM_READ,
    BT_GATT_CHRC_PROP_READ | BT_GATT_CHRC_PROP_NOTIFY,
    &GattClient_onEPollRead,
    nullptr,
    this);
  m_notify_handle = gatt_db_attribute_get_handle(m_blepoll);

  bt_uuid16_create(&uuid, GATT_CLIENT_CHARAC_CFG_UUID);
  gatt_db_service_add_descriptor(
    service,
    &uuid,
    BT_ATT_PERM_READ | BT_ATT_PERM_WRITE,
    &GattClient_onEPollRead,
    nullptr,
    this);

  if (!m_blepoll)
  {
    XLOG_CRITICAL("failed to create ble poll indicator characteristic");
  }

  gatt_db_service_set_active(service, true);
}

void
GattClient::onDataChannelIn(
  gatt_db_attribute*    attr,
  uint32_t              id,
  uint16_t              offset,
  uint8_t const*        data,
  size_t                len,
  uint8_t               UNUSED_PARAM(opcode),
  bt_att*               UNUSED_PARAM(att))
{
  XLOG_INFO("onDataChannelIn(offset=%d, len=%zd)", offset, len);

  // TODO: should this use memory_stream?
  for (size_t i = 0; i < len; ++i)
  {
    char c = static_cast<char>(data[i + offset]);
    m_incoming_buff.push_back(c);

    if (c == kRecordDelimiter)
    {
      if (!m_data_handler)
      {
        // TODO:
        XLOG_WARN("no data handler registered");
      }
      else
      {
        m_incoming_buff.push_back('\0');
        m_data_handler(&m_incoming_buff[0], m_incoming_buff.size());
      }
      m_incoming_buff.clear();
    }
  }

  gatt_db_attribute_write_result(attr, id, 0);
}

void
GattClient::onDataChannelOut(
  gatt_db_attribute*    attr,
  uint32_t              id,
  uint16_t              offset,
  uint8_t               opcode,
  bt_att*               UNUSED_PARAM(att))
{
  XLOG_INFO("onDataChannelOut(id=%d, offset=%u, opcode=%d)",
    id, offset, opcode);

  static int32_t const kBufferSize = 256;
  static uint8_t buff[kBufferSize];

  int n = 0;

  if (offset == 0)
  {
    memset(buff, 0, sizeof(buff));
    n = m_outgoing_queue.get_line((char *)buff, kBufferSize);
  }
  else
  {
    int bytesToWrite = 0;
    while ((bytesToWrite + offset) < kBufferSize && (buff[offset + bytesToWrite] != '\0'))
      bytesToWrite++;

    n = bytesToWrite;
    XLOG_INFO("bytesToWrite:%d offset:%d n:%d", bytesToWrite, offset, n);
  }

  uint8_t const* value = nullptr;
  if (n > 0)
    value = buff + offset;
  else
    buff[0] = '\0';

  // printf("write:%.*s\n", n, (char *) value);
  gatt_db_attribute_read_result(attr, id, 0, value, n);
}

void
GattClient::onEPollRead(
  gatt_db_attribute*    attr,
  uint32_t              id,
  uint16_t              offset,
  uint8_t               opcode,
  bt_att*               UNUSED_PARAM(att))
{
  uint32_t value = m_outgoing_queue.size();

  XLOG_DEBUG("onEPollRead(offset=%d, opcode=%d)", offset, opcode);

  value = htonl(value);
  gatt_db_attribute_read_result(attr, id, 0, reinterpret_cast<uint8_t const *>(&value),
    sizeof(value));
}

void
GattClient::onGapExtendedPropertiesRead(
  struct gatt_db_attribute*     attr,
  uint32_t                      id,
  uint16_t         UNUSED_PARAM(offset),
  uint8_t          UNUSED_PARAM(opcode),
  struct bt_att*   UNUSED_PARAM(att))
{
  uint8_t value[2];
  value[0] = BT_GATT_CHRC_EXT_PROP_RELIABLE_WRITE;
  value[1] = 0;
  gatt_db_attribute_read_result(attr, id, 0, value, sizeof(value));
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
    &GattClient_onGapRead, &GattClient_onGapWrite, this);

  bt_uuid16_create(&uuid, GATT_CHARAC_EXT_PROPER_UUID);
  gatt_db_service_add_descriptor(service, &uuid, BT_ATT_PERM_READ,
    &GattClient_onGapExtendedPropertiesRead, nullptr, this);

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
    uint8_t UNUSED_PARAM(opcode), bt_att* UNUSED_PARAM(att))
{
  XLOG_DEBUG("onGapRead %04x", id);

  uint8_t error = 0;
  size_t len = strlen(ServerName);
  if (offset > len)
  {
    error = BT_ATT_ERROR_INVALID_OFFSET;
  }
  else
  {
    error = 0;
    len -= offset;
    uint8_t const* value = nullptr;
    if (len)
    {
      value = reinterpret_cast<uint8_t const *>(ServerName + offset);
    }
    gatt_db_attribute_read_result(attr, id, error, value, len);
  }
}


void
GattClient::onGapWrite(
  gatt_db_attribute*  attr,
  uint32_t            id,
  uint16_t            offset,
  uint8_t const*      data,
  size_t              len,
  uint8_t             UNUSED_PARAM(opcode),
  bt_att*             UNUSED_PARAM(att))
{
  XLOG_DEBUG("onGapWrite");

  uint8_t error = 0;
  if ((offset + len) == 0)
  {
    memset(ServerName, 0, sizeof(ServerName));
  }
  else if (offset > sizeof(ServerName))
  {
    error = BT_ATT_ERROR_INVALID_OFFSET;
  }
  else
  {
    memcpy(ServerName + offset, data, len);
  }

  gatt_db_attribute_write_result(attr, id, error);
}

void
GattClient::onServiceChanged(gatt_db_attribute* attr, uint32_t id, uint16_t UNUSED_PARAM(offset),
    uint8_t UNUSED_PARAM(opcode), bt_att* UNUSED_PARAM(att))
{
  XLOG_DEBUG("onServiceChanged");
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
      GattClient_onServiceChanged, nullptr, this);

  bt_uuid16_create(&uuid, GATT_CLIENT_CHARAC_CFG_UUID);
  gatt_db_service_add_descriptor(service, &uuid, BT_ATT_PERM_READ | BT_ATT_PERM_WRITE,
      GattClient_onServiceChangedRead, GattClient_onServiceChangedWrite, this);

  gatt_db_service_set_active(service, true);
}

void
GattClient::onServiceChangedRead(gatt_db_attribute* attr, uint32_t id, uint16_t UNUSED_PARAM(offset),
  uint8_t UNUSED_PARAM(opcode), bt_att* UNUSED_PARAM(att))
{
  XLOG_DEBUG("onServiceChangedRead");

  uint8_t value[2] {0x00, 0x00};
  if (m_service_change_enabled)
    value[0] = 0x02;
  gatt_db_attribute_read_result(attr, id, 0, value, sizeof(value));
}

void
GattClient::onServiceChangedWrite(gatt_db_attribute* attr, uint32_t id, uint16_t offset,
    uint8_t const* value, size_t len,
    uint8_t UNUSED_PARAM(opcode), bt_att* UNUSED_PARAM(att))
{
  XLOG_DEBUG("onServiceChangeWrite");

  uint8_t ecode = 0;
  if (!value || (len != 2))
    ecode = BT_ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LEN;

  if (!ecode && offset)
    ecode = BT_ATT_ERROR_INVALID_OFFSET;

  if (!ecode)
  {
    if (value[0] == 0x00)
      m_service_change_enabled = false;
    else if (value[0] == 0x02)
      m_service_change_enabled = true;
    else
      ecode = 0x80;
  }

  gatt_db_attribute_write_result(attr, id, ecode);
}

void
GattClient::addDeviceInfoCharacteristic(
  gatt_db_attribute* service,
  uint16_t           id,
  std::string const& value)
{
  bt_uuid_t uuid;
  bt_uuid16_create(&uuid, id);

  gatt_db_attribute* attr = gatt_db_service_add_characteristic(service, &uuid, BT_ATT_PERM_READ,
    BT_GATT_CHRC_PROP_READ, nullptr, nullptr, this);

  if (!attr)
  {
    XLOG_CRITICAL("failed to create DIS characteristic %u", id);
    return;
  }

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

  // TODO: I don't know what the end value is for. The last call here to add
  // the manufacturer name was failing, but when I upp'ed it from 14 to 30
  // the error went away. Not sure what's going on?
  gatt_db_attribute* service = gatt_db_add_service(m_db, &uuid, true, 30);

  addDeviceInfoCharacteristic(service, kUuidSystemId, DIS_getSystemId());
  addDeviceInfoCharacteristic(service, kUuidModelNumber, DIS_getModelNumber());
  addDeviceInfoCharacteristic(service, kUuidSerialNumber, DIS_getSerialNumber());
  addDeviceInfoCharacteristic(service, kUuidFirmwareRevision, DIS_getFirmwareRevision());
  addDeviceInfoCharacteristic(service, kUuidHardwareRevision, DIS_getHardwareRevision());
  addDeviceInfoCharacteristic(service, kUuidSoftwareRevision, DIS_getSoftwareRevision());
  addDeviceInfoCharacteristic(service, kUuidManufacturerName, DIS_getManufacturerName());

  gatt_db_service_set_active(service, true);
}

void
GattClient::onTimeout()
{
  uint32_t bytes_available = m_outgoing_queue.size();

  if (bytes_available > 0)
  {
    XLOG_INFO("notification of %u bytes available", bytes_available);

    bytes_available = htonl(bytes_available);

    //uint16_t handle = gatt_db_attribute_get_handle(m_blepoll);
    uint16_t handle = m_notify_handle;
    int ret = bt_gatt_server_send_notification(
      m_server,
      handle,
      reinterpret_cast<uint8_t *>(&bytes_available),
      sizeof(uint32_t));

    if (!ret)
    {
      XLOG_WARN("failed to send notification:%d with %u bytes pending",
        ret, bytes_available);
    }
  }

  mainloop_modify_timeout(m_timeout_id, 1000);
}

void
GattClient::run()
{
  m_mainloop_thread = std::this_thread::get_id();

  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGTERM);

  mainloop_run();
}

GattClient::GattClient(int fd)
  : RpcConnectedClient()
  , m_fd(fd)
  , m_att(nullptr)
  , m_db(nullptr)
  , m_server(nullptr)
  , m_mtu(16)
  , m_outgoing_queue(kRecordDelimiter)
  , m_incoming_buff()
  , m_data_channel(nullptr)
  , m_blepoll(nullptr)
  , m_service_change_enabled(false)
  , m_timeout_id(-1)
  , m_mainloop_thread()
  , m_data_handler(nullptr)
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
GattClient::enqueueForSend(char const* buff, int n)
{
  if (!buff)
  {
    XLOG_WARN("trying to enqueue null buffer");
    return;
  }

  if (n <= 0)
  {
    XLOG_WARN("invalid buffer length:%d", n);
    return;
  }

  m_outgoing_queue.put_line(buff, n);
}

void
GattClient::onClientDisconnected(int err)
{
  // TODO: we should stash the remote client as a member of the
  // GattClient so we can print out mac addres of client that
  // just disconnected
  XLOG_INFO("disconnect:%d", err);
  mainloop_quit();
}
