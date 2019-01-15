//
// Copyright [2018] [Comcast, Corp]
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
#ifndef __BLUETOOTH_APPLICATION_H__
#define __BLUETOOTH_APPLICATION_H__

#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QNetworkReply>
#include <QObject>

#include <QtBluetooth/QBluetoothAddress>
#include <QtBluetooth/QBluetoothDeviceInfo>
#include <QtBluetooth/QBluetoothDeviceDiscoveryAgent>
#include <QtBluetooth/QLowEnergyController>
#include <QtBluetooth/QLowEnergyCharacteristic>

#ifdef __GNUC__
#define PRINTF_FORMAT(IDX, FIRST) __attribute__((format(printf, IDX, FIRST)))
#else
#define PRINTF_FORMAT(IDX, FIRAT)
#endif

class BluetoothApplication : public QObject
{
Q_OBJECT

public:
  BluetoothApplication(QBluetoothAddress const& adapter);

  void run(QBluetoothAddress const& addr, QJsonDocument const& req);
  void connectToDevice(QBluetoothDeviceInfo const& info);
  void quit();

signals:
  void introspectionComplete();

private:
  void log(char const* format, ...) PRINTF_FORMAT(2, 3);
  void introspectNextService();
  void readNextCharacteristic();
  void configureDevice(QBluetoothAddress const& addr);

private slots:
  void onDeviceConnected();
  void onDiscoveryFinished();
  void onCharacteristicChanged(QLowEnergyCharacteristic const& c, QByteArray const& newValue);
  void onServiceStateChanged(QLowEnergyService::ServiceState newState);
  void onReadInbox(QLowEnergyCharacteristic const& c, QByteArray const& value);

private:
  QBluetoothDeviceDiscoveryAgent* m_disco_agent;
  QBluetoothAddress               m_adapter;
  QBluetoothDeviceInfo            m_device_info;
  QLowEnergyController*           m_controller;
  QList<QLowEnergyService *>      m_discovered_services;
  QList<QLowEnergyService *>::iterator m_service_iterator;
  QList<QLowEnergyCharacteristic> m_discovered_chars;
  QList<QLowEnergyCharacteristic>::iterator m_chars_iterator;
  bool                            m_is_secure;
  QLowEnergyCharacteristic        m_rpc_inbox;
  QLowEnergyCharacteristic        m_rpc_epoll;
  QLowEnergyService*              m_rpc_service;
  QByteArray                      m_json_request;

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
  QByteArray                      m_incoming_data;
};

#endif

