## Build instructions

### RPIB3+

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
  ```
  
  To avoid compatibility issues install bluez-5.47 and restart bluetooth service with reboot:
  
  ```
  sudo make install
  sudo reboot
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


### iOS App

##### Deployment dependencies

> Before performing a Deployment, it is assumed that the following have been set up:

- Xcode 10 
- OS X 10.13.6 or above
- device/simulator with iOS 10+ 

##### 3rd party Libraries

All libraries are configured in `[src]/iOSClientApp/Podfile`

Using following libraries:

* *RxSwift, RxCocoa, RxDataSource, NSObject+Rx* - RxSwift Community frameworks for reactive programming
* *SwiftyJSON* - JSON decoding/encoding
* *RxBluetoothKit* - reactive wrapper on system CoreBluetooth framework
* *RxGesture* - reactive wrapper on UIKit gestures
* *IQKeyboardManagerSwift* - adjusts UI position to avoid keyboard overlapping with active textfield
* *FGRoute* - determines SSID of connected Wi-Fi network


##### Configuration

Configuration is available in properly list file Configuration.plist, which can be found in `[src]/iOSClientApp/iOSClientApp/Resources` folder/project group.

Available options:

* *logLevel* - the log level (0 - none, 1 - errors, 2 - info, 3 - debug)
* *logIncludeTime* - include time in log messages
* *logPrintConsole* - print log to console as well
* *pollingDelay* - polling delay in ms
* *wifiAPI* - Wi-Fi API service name
* *settingsAPI* - settings API service name
* *autoPair* - auto pair first suitable device
* *deviceAdvertisedUUIDs* - advertised UUIDs of the server
* *ePollUUID* - polling characteristic UUID
* *rpcInboxUUID* - RPC inbox characteristic UUID
* *rpcServiceUUID* - RPC service UUID

Note that API services names are changing in the upstream update.
autoPair and deviceAdvertisedUUIDs are described in detail in validation document.

##### Deployment

- Pods directory should be pulled using the following command (run it from directory containing Podfile):

`$ pod install`

- To build and run the app on a real device you will need to do the following:

On device:

1. Open `[src]/iOSClientApp/iOSClientApp.xcworkspace` in Xcode
2. From the left pane select the iOSClientApp project to open project settings.
4. In project settings General tab (opened by default) -> Identity section change the Bundle Identifier to something unique since app is using entitlements
3. In project settings General tab (opened by default) -> Signing section select your Team from a dropdown. (If your account's team doesn't show up you need to log in Xcode -> Preferences -> Accounts)
4. Normally you should be all set, but if app ID wasn't generated for you, try toggling Automatically manage signing on and off, you may also try toggling off the enabled [options](http://take.ms/sQvwt) from Capabilities tag
5. Select *iOSClientApp* scheme from the top left drop down list.
6. Select a device (when connected) from the top left dropdown list.
7. Click menu Product -> Run (Cmd+R)
    
    
## Run bleconfd and connect app

must be step by step, otherwise it maybe cannot worked as expected.

1. Running bleconf should be done with adapter powered off state. If it is powered on, run `sudo hciconfig hci0`
2. run `sudo ./bleconfd` to startup server on RPI device. 
3. build and run *iOSClientApp* app on iOS device.
4. Note that server ends advertising when connected, so make sure to disconnect any other connected devices.
