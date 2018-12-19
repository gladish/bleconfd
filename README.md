

### About

`bleconfd` is a JSON/RPC based configuration management system that uses BLE as a transport. The intent is to use this application to do out-of-box system setup and post setup diagnostics. 

### Architecture

`bleconfd` at it's core is a JSON/RPC server with pluggable services and transports. The native transport suppuorted is GATT over BLE. The server supports the Device Information Service and a proprietary JSON/RPC service. The Device Infomormation Service can be found in the GATT profile specs here:

https://www.bluetooth.com/specifications/gatt

The setup service defines two GATT characteristics (service uuid -- 503553ca-eb90-11e8-ac5b-bb7e434023e8)
1. Inbox ("510c87c8-eb90-11e8-b3dc-17292c2ecc2d")
1. EPoll ("5140f882-eb90-11e8-a835-13d2bd922d3f")


After making a GATT connection, the client sends JSON/RPC style requests by writing to the Inbox. The request must be terminated with an ASCII Record Separator character, which is 30 in decimal.


https://upload.wikimedia.org/wikipedia/commons/thumb/1/1b/ASCII-Table-wide.svg/875px-ASCII-Table-wide.svg.png

The requests are processed asynchronously. The server replies to the client by making the JSON/RPC response available via reading from the Inbox. The design is similar to using a streaming socket where the client reads and writes streams of data the same connection. The server also periodically notifies on the EPoll characteristic when there is pending data to be read from the Inbox. The server will send a notify on EPoll with an integer indicating the number of pending bytes to be read. The client can read as many bytes as desired but should keep reading until it sees an ASCII Record Separator character in the data. The client can the parse the bytes that have been read (excluding the Record Separator) as plain ASCII/JSON.


### JSON/RPC Usage

The server always expects JSON/RPC request. The format should be very familiar to a regular user of JSON/RPC. A sample request to retrieve the WiFi status looks like:

```
{ "jsonrpc": "2.0", "method": "wifi-get-status", "id": 1234 }
```

Internally, the server expects to see method names separated by dashes. The first token is the RPC Service. In this case, the service name is "wifi". This should not be confused with a Bluetooth Service. The RPC Service is just a collection of similar methods grouped together. In the current implementation, it aactually maps to a c++ class. The actual method invoked is `getStatus`. You can think of this as a strinified version of


```
class WiFiSerice
{
public:
   Response getStatus(Request req); // wifi-get-status
}
```


https://www.jsonrpc.org/specification



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
