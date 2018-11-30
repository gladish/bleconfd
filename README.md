### Goal

The goal of the `bleconfd` service is to communicate between a mobile app and an embedded device (Raspberry Pi 3 for our testing) over Bluetooth.  Messages sent will be in JSON format, using JSON RPC, between the mobile device and the "server" (rPi).

Messages will be small and split into frames.  The server will have an "inbox" where messages are added, and an "outbox", where replies can be placed asynchronously.

### Architecture

![Architecture](https://www.dropbox.com/s/8te3xgw8nef0yil/Screen%20Shot%202018-11-21%20at%205.16.52%20pm.png?raw=1)

### Target

The initial target for the server is a Raspberry Pi running Raspbian.  We want to target the rPi 3 because it has BLE built in.  We will be using BlueZ for the main Bluetooth functionality, including the GATT server.

* GLib  2.38.2
* BlueZ 5.45

### BUILD

1. Set these three
```
export HOSTAPD_HOME=/home/pi/work/hostap
export BLUEZ_HOME=/home/pi/work/bluez-5.47
export CJSON_HOME=/home/pi/work/cJSON
```

2. `make`


### Demos

These are a couple simple demos we hope to build out in the short term:

#### Demo 1

The initial demo will just be a simple service for getting and setting key value pairs via Glib.  We will build:

* A simple service on the server that can accept messages to get / set values based on a give key
 * The set values will write to an ini file via Glib
* A simple mobile app that we can connect to the server via bluetooth to view and set the individual key values

#### Demo 2

The next demo, after demo 1 is complete, will involve setting wifi settings on the Raspberry Pi through the mobile app.  We will be able to enter a mobile network name and an WPA2 password on the mobile app, and this data will be sent over Bluetooth to the Raspberry Pi.  The Raspberry Pi will accept the message, configure it's wifi settings accordingly, and will connect to the given mobile network.

### Complete version

The complete app will eventually have mobile and command line test clients for many different data requests.  We will also build wiki style documentation for request, response, and the notification pieces of the JSON RPC protocol.