
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
#include <QLoggingCategory>

// for testing
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <iostream>

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

void
requires(QString const& arg)
{
  if (!parser.isSet(arg))
    printMissingArg(arg);
}

int main(int argc, char* argv[])
{
  bool debug = false;

  QJsonDocument     jsonRequest = test_getWiFiStatus();
  QBluetoothAddress target;
  QBluetoothAddress adapter;

  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName("BLE RPC Test Client");
  QCoreApplication::setApplicationVersion("1.0");

  parser.setApplicationDescription("Test BLE interface on RDKC xCam2");
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addOptions({
    {{"d", "debug"}, "Enable debugging"},
    {{"t", "target"}, "The BLE MAC address for the target device", "mac"},
    {{"a", "ble-adapter"}, "For computers with multiple BLE adapters, use to set mac", "mac"},
    {{"r", "request"}, "The RPC call to make. This can be path to JSON or actual json string"},
  });
  parser.process(app);

  if (parser.isSet("debug"))
    debug = true;

  // allow for tesing
  if (jsonRequest.isEmpty())
  {
    if (!parser.isSet("request"))
    {
      std::cerr << "operation reqired" << std::endl;
      return 0;
    }


    QString req = parser.value("request");
    if (!fileExists(req))
    {
      std::cerr << "cannot find: " << qPrintable(req) << std::endl;
      return 0;
    }

    jsonRequest = jsonFromFile(req);
  }

  BluetoothApplication bleapp(adapter);

  if (debug)
    QLoggingCategory::setFilterRules(QStringLiteral("qt.bluetooth* = true"));

  target = QBluetoothAddress(parser.value("target"));
  bleapp.run(target, jsonRequest);

  return app.exec();
}

