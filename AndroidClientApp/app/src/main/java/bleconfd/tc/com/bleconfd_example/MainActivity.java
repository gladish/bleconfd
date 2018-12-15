package bleconfd.tc.com.bleconfd_example;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothProfile;
import android.content.Context;
import android.support.v4.app.DialogFragment;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.Spinner;
import android.widget.TextView;

import org.json.JSONException;
import org.json.JSONObject;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Set;
import java.util.UUID;

public class MainActivity extends AppCompatActivity implements DialogSetWifi.DialogListener {
    BluetoothAdapter mBluetoothAdapter = null;

    private TextView logTxt = null;
    private Spinner spinner = null;
    private Button connectBtn = null;
    private Button disConnectBtn = null;
    private Button setWifiBtn = null;
    private final String TAG = "BLE-EXAMPLE";

    private BluetoothDevice currentDevice = null;
    private Context mContext;
    private BLEInstance bLEInstance = null;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        this.mContext = this;

        spinner = findViewById(R.id.pairedBleSpinner);
        logTxt = findViewById(R.id.logTxt);
        connectBtn = findViewById(R.id.connectBtn);
        disConnectBtn = findViewById(R.id.disConnectBtn);
        setWifiBtn = findViewById(R.id.setWifiBtn);


        mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
        if (mBluetoothAdapter == null) {
            // Device does not support Bluetooth
            logTxt.setText("Device does not support Bluetooth ...");
            return;
        }

        initUI();
    }

    /**
     * update buttons status
     */
    private void updateUI() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (currentDevice == null || bLEInstance != null) {
                    connectBtn.setEnabled(false);
                } else {
                    connectBtn.setEnabled(true);
                }

                if (bLEInstance != null) {
                    disConnectBtn.setEnabled(true);
                    spinner.setEnabled(false);
                } else {
                    spinner.setEnabled(true);
                    disConnectBtn.setEnabled(false);
                }

                if (bLEInstance != null && bLEInstance.isFoundService()) {
                    setWifiBtn.setEnabled(true);
                } else {
                    setWifiBtn.setEnabled(false);
                }
            }
        });
    }

    /**
     * init UI
     */
    private void initUI() {
        final ArrayList<String> list = new ArrayList<>();
        list.add("No Devices");

        this.updateUI();

        final Set<BluetoothDevice> pairedDevices = mBluetoothAdapter.getBondedDevices();
        if (pairedDevices.size() <= 0) {
            logTxt.setText("You need paired rpi3 bluetooth device in android system setting first");
            return;
        }

        for (BluetoothDevice device : pairedDevices) {
            list.add(device.getName());
        }

        ArrayAdapter adapter = new ArrayAdapter(this, R.layout.spinner_item, R.id.spinnerTxt, list);
        spinner.setAdapter(adapter);
        spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                Log.d(TAG, "selected position = " + position);
                currentDevice = null;
                for (BluetoothDevice device : pairedDevices) {
                    if (device.getName().equals(list.get(position))) {
                        currentDevice = device;
                    }
                }
                updateUI();
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {

            }
        });


        // connect button button click event
        connectBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                closeInstance();
                logTxt.setText("");
                bLEInstance = new BLEInstance(currentDevice);
                bLEInstance.connect();
                updateUI();
            }
        });

        //disconnect button click event
        disConnectBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                closeInstance();
                logTxt.setText("");
                pushMessage("ble connection closed");
            }
        });

        // set wifi button click event
        setWifiBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                DialogSetWifi dialog = new DialogSetWifi();
                dialog.show(getSupportFragmentManager(), "SetWifiDialog");
            }
        });
    }

    @Override
    public void onBackPressed() {
        moveTaskToBack(true);
        android.os.Process.killProcess(android.os.Process.myPid());
        System.exit(1);
    }

    /**
     * close ble connection instance
     */
    private void closeInstance() {
        if (bLEInstance != null) {
            bLEInstance.close();
        }
        bLEInstance = null;
        updateUI();
    }

    /**
     * show message
     *
     * @param msg the mssage
     */
    private void pushMessage(final String msg) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                logTxt.setText(logTxt.getText() + "\n" + msg);
            }
        });
    }

    /**
     * create
     *
     * @param ssid
     * @param password
     * @return
     * @throws JSONException
     */
    private byte[] connectWifiBody(String ssid, String password) throws JSONException {

        JSONObject wifiObj = new JSONObject();
        wifiObj.put("jsonrpc", "2.0");
        wifiObj.put("method", "wifi-connect");
        wifiObj.put("id", (int) (Math.random() * 100));
        wifiObj.put("wi-fi_tech", "infra");

        JSONObject disco = new JSONObject();
        disco.put("ssid", ssid);
        wifiObj.put("discovery", disco);

        JSONObject creds = new JSONObject();
        creds.put("akm", "psk");
        creds.put("pass", password);
        wifiObj.put("cred", creds);

        BlePacket blePacket = new BlePacket();
        return blePacket.pack(wifiObj.toString());
    }


    @Override
    public boolean onConnect(DialogFragment dialog, String ssid, String password) {
        if (bLEInstance == null) {
            return false;
        }
        try {
            boolean ret = bLEInstance.write(connectWifiBody(ssid, password));
            if (ret) {
                pushMessage("rpc(wifi-connect) invoked, waiting response...");
            }
            return ret;
        } catch (JSONException e) {
            e.printStackTrace();
            return false;
        }
    }

    /**
     * BLE class instance
     */
    class BLEInstance {
        private final BluetoothDevice mmDevice;
        private UUID rpcServiceUUID = UUID.fromString("503553ca-eb90-11e8-ac5b-bb7e434023e8");
        private UUID rpcInboxUUId = UUID.fromString("510c87c8-eb90-11e8-b3dc-17292c2ecc2d");
        private UUID ePollUUID = UUID.fromString("5140f882-eb90-11e8-a835-13d2bd922d3f");

        private BluetoothGatt mBluetoothGatt = null;
        private Thread readThread = null;
        private BlePacket blePacket = new BlePacket();

        BLEInstance(BluetoothDevice device) {
            mmDevice = device;
        }


        /**
         * close BLE instance
         */
        public void close() {
            if (mBluetoothGatt == null) {
                return;
            }
            mBluetoothGatt.disconnect();
            mBluetoothGatt.close();
            cancelReadThread();
            mBluetoothGatt = null;
        }

        /**
         * connect to BLE
         */
        public void connect() {

            mBluetoothGatt = mmDevice.connectGatt(mContext, false, new BluetoothGattCallback() {
                @Override
                public void onCharacteristicRead(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {

                    if (characteristic.getValue() == null || characteristic.getValue().length <= 0) {
                        return;
                    }

                    if (characteristic.getUuid().equals(ePollUUID)) {
                        ByteBuffer byteBuffer = ByteBuffer.allocate(characteristic.getValue().length);
                        byteBuffer.position(0);
                        byteBuffer.put(characteristic.getValue());
                        byteBuffer.position(0);
                        int length = byteBuffer.getInt(0);
                        Log.d(TAG, "notification got, start read ... length = " + length);
                        if (length > 0) {
                            readInbox();
                        }
                    } else if (characteristic.getUuid().equals(rpcInboxUUId)) {
                        pushMessage("receive bytes, length = " + characteristic.getValue().length);
                        blePacket.unPack(characteristic.getValue());
                        while (blePacket.getMessages().size() > 0) {
                            pushMessage(blePacket.getMessages().poll().toString());
                        }
                    }
                    characteristic.setValue(new byte[]{});
                }

                @Override
                public void onCharacteristicWrite(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
                    super.onCharacteristicWrite(gatt, characteristic, status);
                }

                @Override
                public void onServicesDiscovered(BluetoothGatt gatt, int status) {
                    pushMessage("All done, BLE connection create successful. started ePoll thread");
                    ePollThread();
                    updateUI();
                }

                @Override
                public void onCharacteristicChanged(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic) {
                    super.onCharacteristicChanged(gatt, characteristic);
                    pushMessage("onCharacteristicChanged ???");
                }

                @Override
                public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
                    if (newState == BluetoothProfile.STATE_CONNECTED) {
                        pushMessage("start discoverServices...");
                        mBluetoothGatt.discoverServices();
                    } else {
                        pushMessage("connection closed.");
                        closeInstance();
                    }
                }
            }, BluetoothDevice.TRANSPORT_LE);

            if (mBluetoothGatt.connect()) {
                pushMessage("start connecting ...");
            } else {
                pushMessage("connect failed");
                closeInstance();
            }
        }

        /**
         * cancel read thread
         */
        private void cancelReadThread() {
            if (readThread != null) {
                readThread.interrupt();
            }
            readThread = null;
        }

        /**
         * read inbox message
         */
        void readInbox() {
            BluetoothGattCharacteristic readCharacteristic =
                    getCharacteristic(rpcServiceUUID.toString(), rpcInboxUUId.toString());
            mBluetoothGatt.readCharacteristic(readCharacteristic);
        }

        /**
         * start a thread to read message
         */
        void ePollThread() {
            this.readThread = new Thread(new Runnable() {
                @Override
                public void run() {
                    while (true) {
                        try {
                            if (mBluetoothAdapter == null || mBluetoothGatt == null) {
                                return;
                            }
                            if (readThread.isInterrupted()) {
                                return;
                            }
                            Thread.sleep(500);
                            BluetoothGattCharacteristic readCharacteristic =
                                    getCharacteristic(rpcServiceUUID.toString(), ePollUUID.toString());
                            mBluetoothGatt.readCharacteristic(readCharacteristic);
                        } catch (Exception e) {
                            e.printStackTrace();
                            return;
                        }
                    }
                }
            });
            this.readThread.start();
        }

        /**
         * get service
         *
         * @param uuid the service uuid
         */
        BluetoothGattService getService(UUID uuid) {
            if (mBluetoothAdapter == null || mBluetoothGatt == null) {
                return null;
            }
            return mBluetoothGatt.getService(uuid);
        }

        /**
         * get Characteristic
         *
         * @param serviceUUID        the service uuid
         * @param characteristicUUID the Characteristic uuid
         */
        private BluetoothGattCharacteristic getCharacteristic(String serviceUUID, String characteristicUUID) {
            BluetoothGattService service = getService(UUID.fromString(serviceUUID));

            if (service == null) {
                return null;
            }
            final BluetoothGattCharacteristic gattCharacteristic = service.getCharacteristic(UUID.fromString(characteristicUUID));
            if (gattCharacteristic != null) {
                return gattCharacteristic;
            } else {
                pushMessage("Can not find 'BluetoothGattCharacteristic'");
                return null;
            }
        }

        /**
         * is BLE service found or not
         */
        public boolean isFoundService() {
            return mBluetoothGatt != null && mBluetoothGatt.getServices().size() > 0
                    && mBluetoothGatt.getService(rpcServiceUUID) != null;
        }

        /**
         * write data to rpc inbox, we don't care the data size, system will auto split it
         *
         * @param data the binary data
         */
        boolean write(byte[] data) {
            // write into rpi service in box
            BluetoothGattCharacteristic writeCharacteristic = getCharacteristic(rpcServiceUUID.toString(),
                    rpcInboxUUId.toString());
            if (writeCharacteristic == null) {
                pushMessage("Write failed. GattCharacteristic is null.");
                return false;
            }
            writeCharacteristic.setValue(data);
            boolean result = writeCharacteristicWrite(writeCharacteristic);
            pushMessage("write to rpc inbox length = " + data.length + ", result = " + result);
            return result;
        }


        boolean writeCharacteristicWrite(BluetoothGattCharacteristic characteristic) {
            return mBluetoothGatt.writeCharacteristic(characteristic);
        }
    }
}

