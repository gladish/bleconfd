 

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

The requests are processed asynchronously. The server replies to the client by making the JSON/RPC response available via reading from the Inbox. The design is similar to using a streaming socket where the client reads and writes streams of data on the same connection. The server also periodically notifies on the EPoll characteristic when there is pending data to be read from the Inbox. The server will send a notify on EPoll with an integer indicating the number of pending bytes to be read. This should be look familiar to developers who have used read(2), write(2), and select(2). The client can read as many bytes as desired but should keep reading until it sees an ASCII Record Separator character in the data. The client can the parse the bytes that have been read (excluding the Record Separator) as plain ASCII/JSON.

### JSON/RPC Usage

The server always expects JSON/RPC request. The format should be very familiar to a regular user of JSON/RPC. A sample request to retrieve the WiFi status looks like:

```
{ "jsonrpc": "2.0", "method": "wifi-get-status", "id": 1234 }
```

Internally, the server expects to see method names separated by dashes. The first token is the RPC Service. In this case, the service name is "wifi". This should not be confused with a Bluetooth Service. The RPC Service is just a collection of functionally related methods grouped together.



The requests always use named parameters. 


https://www.jsonrpc.org/specification



### Implementation Details

This code was originally developed on Raspberry Pi running Raspian using BlueZ with HCI and c++ 11. The code is strucuted in such a way that it should be easy to provide additional transports like TCP, other BLE APIs, etc. More importantly, the code is structured such that additional RPC functionality can be compiled into the application. This is to allow for more JSON/RPC methods to be supported without having to know much about BLE.

#### JSON/RPC Method Name Mapping

The JSON/RPC method names are token separated by dashes. I.e `net-get-interfaces`. The structure always follows the same convention. The JSON/RPC method name is constructed by concatenating the RPC Service name, a dash, and the RPC method name. Although there is nothing technically requiring this, it keeps a consistent naming convention by placing the name followed by the verb.

Developers extending the application with new RPC services should follow the same naming convetions. 


```
class WiFiSerice
{
public:
   Response getStatus(Request req); // wifi-get-status
}
```

* BlueZ 5.45

### BUILD

1. Set these three
```
export HOSTAPD_HOME=/home/pi/work/hostap
export BLUEZ_HOME=/home/pi/work/bluez-5.47
export CJSON_HOME=/home/pi/work/cJSON
```

2. `make`
