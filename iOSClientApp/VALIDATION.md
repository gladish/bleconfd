## Build & Run

- Follow the guide in **README.md**
- In order to use app services your device needs to be paired with "XPI-SETUP" (you can change the name in bleconf/bleconfd.ini)
- Note that name takes time to be propagated, you might need to toggle your bluetooth adapter on RPI on/off
- You can toggle RPI bluetooth adapter off/on with `sudo hciconfig hci0 down`/`sudo hciconfig hci0 up` or system GUI
- After restarting bluetooth, start server again with `sudo ./bleconfd`


## Verification Notes

As a reference, build the android app from the bleconfd repo following the Readme there.

iOS has different Bluetooth stack and is clearly having troubles with pairing to the server (Android has troubles too, but at least the bonding state persists).

Hence the app utilizes both list of paired devices and a scanning.

Clicking dropdown on home screen will restart scan.
Note about default configuration limiting scan to the bleconfd advertised service. Bleconfd advertising script is configured incorrectly but I used the value it actually advertises, so in most cases you will see just the server if it is running, however you may see other unexpected peripherals.

*autoPair* enabled will automatically select first scanned or paired device, and if that gives a bad result for you - you can disable it in configuration. 

You can also remove the scanning filter key *deviceAdvertisedUUIDs* to see all scanned devices instead of compatible ones.

In my case enabling both just lets me connect to device hassle-free (can seen on video) without prior pairing which usualy takes a dozen attempts (both on Android and on iOS).

In order to perform pairing you need to open Settings app -> Bluetooth and wait for Xfinity.Xcam2 to appear. Restart the server on RPI and scan on Bluetooth iOS device if you see a different name like 'BlueZ 5.xx'. Note that after pairing iOS assigns a different name. In my case it was 'TheUnknownServer'


[#3](https://github.com/jmgasper/bleconfd/issues/3)

You can set Wi-Fi and get current status.

In order for wpa-supplicant to work reliably you may need to clear the existing connections.

Open '/etc/wpa_supplicant/wpa_supplicant.conf' in a text editor (sudo `nano /etc/wpa_supplicant/wpa_supplicant.conf`) and remove all 'network' dictionaries (keep the header describing control interface intact).

In the info level log visible on Wi-Fi screen you will see a message about request being sent and then a message with the request status.

Test the error cases (add group with existing name, rename group to existing name) and check that server error is propagated to UI.

[#9](https://github.com/jmgasper/bleconfd/issues/9)
INI editing is following Android implementation: adding/editing/removing groups/values. App tries to present content in same order as in server response.

After each modification app grabs fresh data from server following Android implementation

[#11](https://github.com/jmgasper/bleconfd/issues/11)
Open INI page, load data and swipe cells left to edit and right to delete. You can also see how they work in the video

[#12](https://github.com/jmgasper/bleconfd/issues/12)
Since iOS12 app are required to include Access WiFi Information capability, if you followed build steps in **README.md** correctly, you should be all set and determining SSID should be available.
You can see in the video app is correctly prefilling the connected Wi-Fi


[#15](https://github.com/jmgasper/bleconfd/issues/15)
Do some actions and go back to log screen to see the logs.
Logger is using 3 level (+ 1 level for off) of logs: error, info and debug.
The Home & Wi-Fi screens are showing info log for this screen.
Logs screen is displaying all logs.
You can toggle on/off time displayed for each event and you can toggle on/off printing logs in Xcode debug console.

###Additional Notes

Check out the BLE API implementation, it lets robust communication with bleconfd server and is easily scalable to with new services.

Wi-Fi and INI file screens are disabled when app is not connected. That means you will be navigated back to Home if server is disconnected while you are on one of those screens. 

## Video

[http://take.ms/MZ7hV](http://take.ms/MZ7hV)


