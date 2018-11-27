#ifndef __BLUETOOTH_APPLICATION_H__
#define __BLUETOOTH_APPLICATION_H__

#include <QJsonObject>
#include <QList>
#include <QNetworkReply>
#include <QObject>

#include <QtBluetooth/QBluetoothAddress>
#include <QtBluetooth/QBluetoothDeviceInfo>
#include <QtBluetooth/QBluetoothDeviceDiscoveryAgent>
#include <QtBluetooth/QLowEnergyController>

#ifdef __GNUC__
#define PRINTF_FORMAT(IDX, FIRST) __attribute__((format(printf, IDX, FIRST)))
#else
#define PRINTF_FORMAT(IDX, FIRAT)
#endif


extern QString const kXboAccountId;
extern QString const kXboDeviceId;
extern QString const kClientSecret;


class BluetoothApplication : public QObject
{
Q_OBJECT

public:
  BluetoothApplication(QBluetoothAddress const& adapter);

  void run(QBluetoothAddress const& addr);
  void connectToDevice(QBluetoothDeviceInfo const& info);
  void quit();

signals:
  void introspectionComplete();

private:
  void log(char const* format, ...) PRINTF_FORMAT(2, 3);
  void introspectNextService();
  void readNextCharacteristic();
  void pollStatus();
  void configureDevice(QBluetoothAddress const& addr);

private slots:
  void onDeviceConnected();
  void onDiscoveryFinished();
  void onCharacteristicChanged(QLowEnergyCharacteristic const& c, QByteArray const& newValue);
  void onServiceStateChanged(QLowEnergyService::ServiceState newState);

private:
  QBluetoothDeviceDiscoveryAgent* m_disco_agent;
  QBluetoothAddress               m_adapter;
  QBluetoothDeviceInfo            m_device_info;
  QLowEnergyController*           m_controller;
  QList<QLowEnergyService *>      m_discovered_services;
  QList<QLowEnergyService *>::iterator m_service_iterator;
  QList<QLowEnergyCharacteristic> m_discovered_chars;
  QList<QLowEnergyCharacteristic>::iterator m_chars_iterator;
  QString                         m_wifi_config;
  QLowEnergyService*              m_rdk_service;
  QLowEnergyCharacteristic        m_rdk_status_char;
  QLowEnergyCharacteristic        m_rdk_wifi_char;
  bool                            m_poll_status;
  QJsonObject                     m_json_info;
  QString                         m_xbo_account_id;
  QString                         m_client_secret;
  int                             m_proto_version;
  bool                            m_use_pubkey;
  bool                            m_is_secure;
  QString                         m_public_key;
  QNetworkAccessManager*          m_network_access_manager;

  struct TimeSpan
  {
    QDateTime StartTime;
    QDateTime EndTime;
    bool      Ended;
  };

  void startOp(char const* op);
  void endOp(char const* op);
  void printTimes();

  QMap< QString, TimeSpan >       m_times;
  bool                            m_do_full_provision;
  QString                         m_xbo_device_id;
};

#endif

