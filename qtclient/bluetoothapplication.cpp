#include "bluetoothapplication.h"

#include <QtBluetooth/QBluetoothHostInfo>
#include <QtBluetooth/QBluetoothLocalDevice>

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>

#include <iostream>

#define BT_UUID(NAME, UUID) static QBluetoothUuid const NAME = QBluetoothUuid(QString(UUID))

BT_UUID(kRdkRpcInboxChar, "510c87c8-eb90-11e8-b3dc-17292c2ecc2d");
BT_UUID(kRdkRpcEPollChar, "5140f882-eb90-11e8-a835-13d2bd922d3f");
BT_UUID(kRpcRpcService,   "503553ca-eb90-11e8-ac5b-bb7e434023e8");

static QString serviceStateToString(QLowEnergyService::ServiceState s)
{
  if (s == QLowEnergyService::InvalidService)
    return "InvalidService";
  if (s == QLowEnergyService::DiscoveryRequired)
    return "DiscoveryRequired";
  if (s == QLowEnergyService::DiscoveringServices)
    return "DiscoveringServices";
  if (s == QLowEnergyService::ServiceDiscovered)
    return "ServiceDiscovered";
  if (s == QLowEnergyService::LocalService)
    return "LocalService";
  return "Unknown Service State";
  
}

static QString propertiesToString(QLowEnergyCharacteristic::PropertyTypes props)
{
  QStringList list;
  if (props & QLowEnergyCharacteristic::Unknown)
    list.append("Unknown");
  if (props & QLowEnergyCharacteristic::Broadcasting)
    list.append("Broadcasting");
  if (props & QLowEnergyCharacteristic::Read)
    list.append("Read");
  if (props & QLowEnergyCharacteristic::WriteNoResponse)
    list.append("WriteNoResponse");
  if (props & QLowEnergyCharacteristic::Write)
    list.append("Write");
  if (props & QLowEnergyCharacteristic::Notify)
    list.append("Notify");
  if (props & QLowEnergyCharacteristic::Indicate)
    list.append("Indicate");
  if (props & QLowEnergyCharacteristic::WriteSigned)
    list.append("WriteSigned");
  if (props & QLowEnergyCharacteristic::ExtendedProperty)
    list.append("ExtendedProperty");
  return "[" + list.join(',') + "]";
}

#if 0
static QMap<QBluetoothUuid, QString> kRDKCharacteristicUUIDs = 
{
  { kRdkProvisionStateCharacteristic, "Provision State"},
  { kRdkPublicKeyCharacteristic, "Public Key"},
  { kRdkWiFiConfigCharacteristic, "WiFi Config"},

  { QBluetoothUuid(QString("12984c43-3b43-4952-a387-715dcf9795c6")), "DeviceId"},
  { QBluetoothUuid(QString("16abb396-ab2c-4928-973d-e28a406d042b")), "Lost And Found Access Token"}
};
#endif

static uint16_t
ST(uint16_t n)
{
  return n;
}

static QMap< uint16_t, QString > kRdkStatusStrings = 
{
  { ST(0x0101), "Awaiting WiFi Configuration" },
  { ST(0x0102), "Processing WiFi Configuration" },
  { ST(0x0103), "Connecting to WiFi" },
  { ST(0x0104), "Successfully conected to WiFi" },
  { ST(0x0105), "Acquiring IP Address" },
  { ST(0x0106), "IP address acquired" },
  { ST(0x09ff), "Setup complete" },
  
  { ST(0x0a01), "Unknown Error" },
  { ST(0x0a02), "Error decrytingA WiFi configuration" },
  { ST(0x0a03), "Malformed WiFi configuration" },
  { ST(0x0a04), "Acquire IP timeout" }
};

QString
statusToString(QByteArray const& arr)
{
  uint16_t n = arr[1];
  n <<= 8;
  n |= arr[0];

  QString s;
  QMap< uint16_t, QString >::const_iterator i = kRdkStatusStrings.find(n);
  if (i != kRdkStatusStrings.end())
  {
    s = QString("(%1) %2").arg(QString::asprintf("0x%04x",n)).arg(i.value());
  }
  else
    s = QString::number(n);

  return s;
}

static QString charName(QLowEnergyCharacteristic const& c)
{
  QString name = c.name();
//  if (name.isEmpty() && kRDKCharacteristicUUIDs.contains(c.uuid()))
//    name = kRDKCharacteristicUUIDs[c.uuid()];
  return name;
}

static QString
byteArrayToString(QBluetoothUuid const& uuid, QByteArray const& a)
{
  QString s;

  bool allAscii = true;
  for (char c : a)
  {
    if (!QChar::isPrint(c))
    {
      allAscii = false;
      break;
    }
  }

  if (allAscii)
    s = QString::fromLatin1(a);
  else
    s = a.toHex(',');

  return s;
}


BluetoothApplication::BluetoothApplication(QBluetoothAddress const& adapter)
  : m_disco_agent(nullptr)
  , m_adapter(adapter)
  , m_controller(nullptr)
  , m_do_full_provision(false)
{
  log("local adapter is null:%d", m_adapter.isNull());
  foreach (QBluetoothHostInfo info, QBluetoothLocalDevice::allDevices())
  {
    QBluetoothLocalDevice adapter(info.address());
    log("local adapter:%s/%s", qPrintable(info.name()), qPrintable(info.address().toString()));
  }
}

void
BluetoothApplication::run(QBluetoothAddress const& addr, QJsonDocument const& req)
{
  m_json_request = req.toJson(QJsonDocument::Compact);
  configureDevice(addr);
}

void
BluetoothApplication::configureDevice(QBluetoothAddress const& addr)
{
  if (!m_adapter.isNull())
    m_disco_agent = new QBluetoothDeviceDiscoveryAgent(m_adapter);
  else
    m_disco_agent = new QBluetoothDeviceDiscoveryAgent();

  connect(m_disco_agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
    [=](QBluetoothDeviceInfo const& info)
    {
      if (info.address() == addr)
      {
        log("found match with addr:%s", qPrintable(info.address().toString()));
        m_disco_agent->stop();
        connectToDevice(info);
      }
    });

  startOp("discovery");
  m_disco_agent->start();
}

void
BluetoothApplication::connectToDevice(QBluetoothDeviceInfo const& info)
{
  log("connecting to:%s", qPrintable(info.address().toString())); 

  m_device_info = info;
  m_controller = QLowEnergyController::createCentral(m_device_info);
  connect(m_controller, &QLowEnergyController::connected, this,
    &BluetoothApplication::onDeviceConnected);
  startOp("connect");
  m_controller->connectToDevice();
}

void
BluetoothApplication::onDeviceConnected()
{
  endOp("connect");
  connect(m_controller, &QLowEnergyController::discoveryFinished, this,
    &BluetoothApplication::onDiscoveryFinished);
  connect(m_controller, QOverload<QLowEnergyController::Error>::of(&QLowEnergyController::error),
    [this](QLowEnergyController::Error err) { log("controller error:%d", err); });

  startOp("discovery");
  m_controller->discoverServices();
}

void
BluetoothApplication::onDiscoveryFinished()
{
  endOp("discovery");
  for (QBluetoothUuid const& uuid : m_controller->services())
  {
    QLowEnergyService* service = m_controller->createServiceObject(uuid);

    if (uuid == kRpcRpcService)
    {
      log("found rpc service");
      m_rpc_service = service;
    }

    log("found service uuid:%s name:%s", qPrintable(service->serviceUuid().toString()),
      qPrintable(service->serviceName()));

    m_discovered_services.append(service);
  }

  m_service_iterator = m_discovered_services.begin();
  introspectNextService();
}

void
BluetoothApplication::introspectNextService()
{
  if (m_service_iterator != m_discovered_services.end())
  {
    QLowEnergyService* s = *m_service_iterator;

    QString msg = QString::asprintf("introspec %s/%s", qPrintable(s->serviceUuid().toString()),
      qPrintable(s->serviceName()));
    startOp(qPrintable(msg));
    connect(s, &QLowEnergyService::stateChanged, this, &BluetoothApplication::onServiceStateChanged);
    connect(s, QOverload<QLowEnergyService::ServiceError>::of(&QLowEnergyService::error),
      [this](QLowEnergyService::ServiceError err)
      {
        log("QLowEnergyService Error:%d", err);
      });

    s->discoverDetails();
  }
  else
  {
    log("done discovering services and characteristics");

    // clear all connected slots
    for (auto svc : m_discovered_services)
      svc->disconnect();

    // connect notification
    connect(m_rpc_service, &QLowEnergyService::characteristicChanged,
      this, &BluetoothApplication::onCharacteristicChanged);

    QLowEnergyDescriptor notification = m_rpc_epoll.descriptor(
      QBluetoothUuid::ClientCharacteristicConfiguration);
    m_rpc_service->writeDescriptor(notification, QByteArray::fromHex("0100"));

    if (m_rpc_service)
    {
      m_json_request.append(30);
      m_rpc_service->writeCharacteristic(m_rpc_inbox, m_json_request,
        QLowEnergyService::WriteWithResponse);
    }
    else
    {
      log("rpc service is null, can't send request");
    }
  }
}

void
BluetoothApplication::readNextCharacteristic()
{
  if (m_chars_iterator == m_discovered_chars.end())
  {
    m_discovered_chars.clear();
    m_service_iterator++;
    introspectNextService();
    return;
  }

  QLowEnergyService* s = *m_service_iterator;
  QLowEnergyCharacteristic c = *m_chars_iterator;

  if (c.properties() & QLowEnergyCharacteristic::Read)
  {
    m_chars_iterator++;
    s->readCharacteristic(c);
  }
  else
  {
    log("property is not readable");
    QString name = charName(c);
    log("\tcharacteristic uuid:%s name:%s", qPrintable(c.uuid().toString()), qPrintable(name));
    log("\t\tprops:%s", qPrintable(propertiesToString(c.properties())));
    log("\t\tvalue: [not readable]");
    m_chars_iterator++;
    QTimer::singleShot(100, [this] { readNextCharacteristic(); });
  }
}

void
BluetoothApplication::onServiceStateChanged(QLowEnergyService::ServiceState newState)
{
  if (newState != QLowEnergyService::ServiceDiscovered)
  {
    log("unhandled service state:%s", qPrintable(serviceStateToString(newState)));
    return;
  }

  QLowEnergyService* s = *m_service_iterator;

  QString msg = QString::asprintf("introspec %s/%s", qPrintable(s->serviceUuid().toString()),
    qPrintable(s->serviceName()));
  endOp(qPrintable(msg));

  connect(s, &QLowEnergyService::characteristicRead, [=](QLowEnergyCharacteristic const& c, QByteArray const& value) {
    QString name = charName(c);
    log("\tcharacteristic uuid:%s name:%s", qPrintable(c.uuid().toString()), qPrintable(name));
    log("\t\tprops:%s", qPrintable(propertiesToString(c.properties())));
    log("\t\tvalue:%s", qPrintable(byteArrayToString(c.uuid(), value)));

    if (c.uuid() == kRdkRpcInboxChar)
    {
      log("found inbox characterisitc");
      m_rpc_inbox = c;
    }
    else if (c.uuid() == kRdkRpcEPollChar)
    {
      log("found epoll characterisitc");
      m_rpc_epoll = c;
    }

    readNextCharacteristic();
  });

  m_discovered_chars.clear();
  for (QLowEnergyCharacteristic const& c : s->characteristics())
    m_discovered_chars.append(c);
  m_chars_iterator = m_discovered_chars.begin();

  readNextCharacteristic();
}

void
BluetoothApplication::log(char const* format, ...)
{
  QDateTime now = QDateTime::currentDateTime();
  printf("%s : ", qPrintable(now.toString(Qt::ISODate)));

  va_list args;
  va_start(args, format);
  vprintf(format, args);
  printf("\n");
  va_end(args);
}

void
BluetoothApplication::onCharacteristicChanged(QLowEnergyCharacteristic const& c, QByteArray const& value)
{
  QString name = c.name();
  log("characterisitic CHANGED uuid:%s name:%s", qPrintable(c.uuid().toString()),
    qPrintable(name));
  log("\tprops:%s", qPrintable(propertiesToString(c.properties())));
  log("\tvalue:%s", qPrintable(byteArrayToString(c.uuid(), value)));
}

void
BluetoothApplication::startOp(char const* op)
{
  log("[%s] -- starting", op);

  QString key = QString::fromUtf8(op);
  if (m_times.contains(key))
    return;

  TimeSpan& span = m_times[key];
  span.StartTime = QDateTime::currentDateTimeUtc();
  span.Ended = false;
}

void
BluetoothApplication::endOp(char const* op)
{
  log("[%s] -- complete", op);

  QString key = QString::fromUtf8(op);
  TimeSpan& span = m_times[key];
  span.EndTime = QDateTime::currentDateTimeUtc();
  span.Ended = true;
}

void
BluetoothApplication::quit()
{
  if (m_controller)
  {
    log("disconnecting from device");
    m_controller->disconnectFromDevice();
  }
  printTimes();
  QCoreApplication::quit();
}

void
BluetoothApplication::printTimes()
{
  std::cout << "--------------- BEGIN TIMINGS ---------------" << std::endl;
  for (auto key : m_times.keys())
  {
    TimeSpan const& span = m_times.value(key);
    if (span.Ended)
    {
      qint64 end = span.EndTime.toMSecsSinceEpoch();
      qint64 start = span.StartTime.toMSecsSinceEpoch();
      printf("%s:%lldms\n", qPrintable(key), (end - start));
    }
    else
    {
      printf("%s: Never Finished\n", qPrintable(key));
    }
  }
  std::cout << "---------------  END TIMINGS ---------------" << std::endl;
}
