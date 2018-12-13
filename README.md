### Goal

The goal of the `bleconfd` service is to communicate between a mobile app and an embedded device (Raspberry Pi 3 for our testing) over Bluetooth.  Messages sent will be in JSON format, using JSON RPC, between the mobile device and the "server" (rPi).

Messages will be small and split into frames.  The server will have an "inbox" where messages are added, and an "outbox", where replies can be placed asynchronously.

### Architecture


### Target

The initial target for the server is a Raspberry Pi running Raspbian.  We want to target the rPi 3 because it has BLE built in.  We will be using BlueZ for the main Bluetooth functionality, including the GATT server.

* BlueZ 5.45

## Build instructions

##### DOWNLOAD AND BUILD LIBS

- update system and install needed libs 

  ```
  sudo apt-get update
  sudo apt install libglib2.0-dev libdbus-1-dev libudev-dev libical-dev libreadline-dev
  ```

- download and build bluez-5.47

  ```
  wget http://www.kernel.org/pub/linux/bluetooth/bluez-5.47.tar.xz
  tar -xvf bluez-5.47.tar.xz
  cd bluez-5.47
  ./configure
  make -j4
  cd ..
  ```

- download and build cJSON

  ```
  git clone https://github.com/DaveGamble/cJSON.git
  cd cJSON
  make
  ```
  then use `sudo vi /etc/ld.so.conf` open ld conf file, add `/home/pi/work/cJSON` to last line, and save it.

  then use `sudo ldconfig /etc/ld.so.conf` to make it effective

- download hostap(wpa_supplicant-2.6), don't need build it

  ```
  cd ~/work
  wget https://w1.fi/releases/wpa_supplicant-2.6.tar.gz
  tar -xvf wpa_supplicant-2.6.tar.gz
  ```

##### BUILD bleconfd

- copy bleconfd folder to  `/home/pi/work`, then goto this folder `cd /home/pi/work/bleconfd `
- use `source env.sh` export all libs path.
- then just run `make`



### Run bleconfd and connect

must be step by step, otherwise it maybe cannot worked as expected.

1. run `sudo ./bleconfd`  to startup server on RPI device.
2. run `sudo hciconfig hci0 piscan ` or click top left bluetooth icon then click "Make discoverable" to make the BLE device discoverable on RPI device.
3. use android 8.0 + phone paired "RPI-BLE*" (you can change the name in bleconfd.ini), you maybe need confirm pair request on RPI device.  **in the first time, the BLE name maybe not this, please wait some times or refresh devices.**
4. install *app-debug.apk* and open it or use source code *AndroidClientApp* run app on android phone.
5. select "RPI-BLE*", then click "CONNECT" button

