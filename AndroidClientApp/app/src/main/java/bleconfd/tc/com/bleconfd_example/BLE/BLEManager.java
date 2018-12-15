package bleconfd.tc.com.bleconfd_example.BLE;


import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothProfile;
import android.content.Context;
import android.util.Log;

import java.nio.ByteBuffer;
import java.util.UUID;

/**
 * BLE manager, singleton pattern
 */
public class BLEManager {

    private static BLEManager that = null;
    private BLELogger bleLogger = null;
    private Context mContext;
    private BluetoothDevice mmDevice = null;


    private UUID rpcServiceUUID = UUID.fromString("503553ca-eb90-11e8-ac5b-bb7e434023e8");
    private UUID rpcInboxUUId = UUID.fromString("510c87c8-eb90-11e8-b3dc-17292c2ecc2d");
    private UUID ePollUUID = UUID.fromString("5140f882-eb90-11e8-a835-13d2bd922d3f");

    private String TAG = "BLEManager";

    private BluetoothGatt mBluetoothGatt = null;
    private Thread readThread = null;
    private BLEPacket blePacket = new BLEPacket();
    private BluetoothAdapter mBluetoothAdapter = null;
    private BLEListener bleListener = null;


    private BLEManager() {

    }

    /**
     * get BLE manager instance
     */
    public static BLEManager getInstance() {
        if (that == null) {
            that = new BLEManager();
        }
        return that;
    }

    /**
     * set logger
     *
     * @param logger the ble logger
     */
    public void setLogger(BLELogger logger) {
        this.bleLogger = logger;
    }

    /**
     * debug message
     *
     * @param msg the message
     */
    private void pushMessage(String msg) {
        if (this.bleLogger != null) {
            this.bleLogger.debug(msg);
        }
    }


    public void init(Context context, BluetoothDevice bluetoothDevice) {
        if (mmDevice != null && bluetoothDevice.getAddress().equals(mmDevice.getAddress())) {
            return;
        }

        this.close();
        this.mContext = context;
        this.mmDevice = bluetoothDevice;
        this.mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
    }

    /**
     * get current device
     *
     * @return the current device
     */
    public BluetoothDevice getMmDevice() {
        return mmDevice;
    }

    /**
     * check the ble is closed or not
     *
     * @return
     */
    public boolean isClosed() {
        return mBluetoothGatt == null;
    }

    /**
     * set ble listener
     *
     * @param bleListener the ble listener
     */
    public void setBleListener(BLEListener bleListener) {
        this.bleListener = bleListener;
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
        if (bleListener != null) {
            bleListener.onClose();
        }
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
                    if (length > 0) {
                        Log.d(TAG, "got content notification, content length = " + length);
                        readInbox();
                    }
                } else if (characteristic.getUuid().equals(rpcInboxUUId)) {
                    pushMessage("receive bytes, length = " + characteristic.getValue().length);
                    blePacket.unPack(characteristic.getValue());
                    while (blePacket.getMessages().size() > 0) {
                        if (bleListener != null) {
                            bleListener.onMessage(blePacket.getMessages().poll());
                        }
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
                if (bleListener != null) {
                    bleListener.onUpdate();
                }
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
                    close();
                }
            }
        }, BluetoothDevice.TRANSPORT_LE);

        if (mBluetoothGatt.connect()) {
            pushMessage("start connecting ...");
        } else {
            pushMessage("connect failed");
            close();
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
                        Thread.sleep(1000);
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
    public boolean write(byte[] data) {
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
