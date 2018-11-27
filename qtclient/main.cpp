
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QLoggingCategory>

#include <iostream>

#include "bluetoothapplication.h"

static QCommandLineParser parser;

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

void
requires(QString const& arg)
{
  if (!parser.isSet(arg))
    printMissingArg(arg);
}

int main(int argc, char* argv[])
{
  bool debug = false;

  QBluetoothAddress target;
  QBluetoothAddress adapter;

  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName("RDKC BLE Test Client");
  QCoreApplication::setApplicationVersion("1.0");

  parser.setApplicationDescription("Test BLE interface on RDKC xCam2");
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addOptions({
    {{"d", "debug"}, "Enable debugging"},
    {{"t", "target"}, "The BLE MAC address for the target device", "mac"},
    {{"a", "ble-adapter"}, "For computers with multiple BLE adapters, use to set mac", "mac"},
  });
  parser.process(app);

  if (parser.isSet("debug"))
    debug = true;

  BluetoothApplication bleapp(adapter);

  if (debug)
    QLoggingCategory::setFilterRules(QStringLiteral("qt.bluetooth* = true"));

  target = QBluetoothAddress(parser.value("target"));

  bleapp.run(target); 
  return app.exec();
}

QString const kXboAccountId("xbo-account-id");
QString const kXboDeviceId("xbo-device-id");
QString const kClientSecret("client-secret");
