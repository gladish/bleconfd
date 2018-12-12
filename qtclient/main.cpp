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
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
#include <QLoggingCategory>

// for testing
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <iostream>
#include <sstream>

#include "bluetoothapplication.h"

static QCommandLineParser parser;

bool
fileExists(QString const& path)
{
  QFileInfo info(path);
  return info.exists() && info.isFile();
}

void
printMissingArg(QString const& s)
{
  std::cout << std::endl;
  std::cout << "missing argument: ";
  std::cout << qPrintable(s) << std::endl << std::endl;
  std::cout << qPrintable(parser.helpText()) << std::endl;
  std::cout << std::endl;
  exit(1);
}

QJsonDocument
jsonFromFile(QString const& fname)
{
  QFile file;
  file.setFileName(fname);
  file.open(QIODevice::ReadOnly | QIODevice::Text);

  QString jsonText = file.readAll();
  return QJsonDocument::fromJson(jsonText.toUtf8());
}

QJsonDocument test_getWiFiStatus()
{
  QJsonDocument doc;
  QJsonObject obj;
  obj["jsonrpc"] = "2.0";
  obj["method"] = "wifi-get-status";
  obj["id"] = 1;
  doc.setObject(obj);
  return doc;
}

QJsonDocument test_getNetworkInterfaces()
{
  QJsonDocument doc;
  QJsonObject obj;
  obj["jsonrpc"] = "2.0";
  obj["method"] = "net-get-interfaces";
  obj["id"] = 2;
  doc.setObject(obj);
  return doc;
}

QJsonDocument test_WiFiConnect()
{
  QJsonDocument doc;
  QJsonObject obj;
  obj["jsonrpc"] = "2.0";
  obj["method"] = "wifi-connect";
  obj["id"] = 2;

  QJsonObject params;
  params["wi-fi_tech"] = "infra";

  QJsonObject discovery;
  discovery["ssid"] = "";
  params["discovery"] = discovery;

  QJsonObject cred;
  cred["akm"] = "psk";
  cred["pass"] = "";
  params["cred"] = cred;

  obj["params"] = params;
  doc.setObject(obj);
  return doc;
}

QJsonDocument getTest(QCommandLineParser& parser, QMap< QString, std::function<QJsonDocument()> >& testCases)
{
  QJsonDocument jsonRequest;

  if (parser.isSet("request"))
  {
    QString req = parser.value("request");
    if (!fileExists(req))
    {
      std::cerr << "cannot find: " << qPrintable(req) << std::endl;
      return jsonRequest;
    }

    jsonRequest = jsonFromFile(req);
  }
  else if (parser.isSet("test"))
  {
    QString testCase = parser.value("test");
    auto iter = testCases.find(testCase);
    if (iter == testCases.end())
    {
      std::cerr << "test case:" << qPrintable(testCase) << " not found" << 
        std::endl;
      return jsonRequest;
    }

    jsonRequest = iter.value()();
  }

  return jsonRequest;
}

void
requires(QString const& arg)
{
  if (!parser.isSet(arg))
    printMissingArg(arg);
}

int main(int argc, char* argv[])
{
  bool debug = false;

  QJsonDocument jsonRequest;
  QBluetoothAddress target;
  QBluetoothAddress adapter;

  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName("BLE RPC Test Client");
  QCoreApplication::setApplicationVersion("1.0");

  QMap< QString, std::function<QJsonDocument ()> > testCases
  {
    {"get-wifi-status", test_getWiFiStatus},
    {"wifi-connect", test_WiFiConnect},
    {"net-get-interfaces", test_getNetworkInterfaces}
  };

  parser.setApplicationDescription("Test BLE interface on RDKC xCam2");
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addOptions({
    {{"d", "debug"}, "Enable debugging"},
    {{"t", "target"}, "The BLE MAC address for the target device", "mac"},
    {{"a", "ble-adapter"}, "For computers with multiple BLE adapters, use to set mac", "mac"},
    {{"r", "request"}, "The RPC call to make. This can be path to JSON or actual json string"},
    {{"l", "list"}, "List available test cases"},
    {{"z", "test"}, "Run a given test", "test"}
  });
  parser.process(app);

  if (parser.isSet("list"))
  {
    std::cout << std::endl;
    std::cout << "Available test cases" << std::endl;
    for (QString const& s : testCases.keys())
      std::cout << "\t" << qPrintable(s) << std::endl;
    std::cout << std::endl;
    return 0;
  }

  if (parser.isSet("debug"))
    debug = true;

  if (!parser.isSet("test"))
  {
    std::cout << "please provide test case with --test" << std::endl;
    return 0;
  }

  jsonRequest = getTest(parser, testCases);
  if (jsonRequest.isEmpty())
  {
    std::cerr << "invalid json request" << std::endl;
    return 0;
  }

  if (debug)
    QLoggingCategory::setFilterRules(QStringLiteral("qt.bluetooth* = true"));

  target = QBluetoothAddress(parser.value("target"));
  if (target.isNull())
  {
    std::cerr << "invalid bluetooth address:" << qPrintable(parser.value("target"))
      << std::endl;
    return 0;
  }

  BluetoothApplication bleapp(adapter);
  bleapp.run(target, jsonRequest);
  return app.exec();
}
