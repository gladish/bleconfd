//
// Copyright [2020] [Comcast Cable]
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
package com.comcast.rdk.management;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothProfile;
import android.content.Context;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;

import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;

public class JsonRpcClient extends BluetoothGattCallback {
  private static final String TAG = "JsonRpcClient";
  private static final UUID kUuidRpcService = UUID.fromString("503553ca-eb90-11e8-ac5b-bb7e434023e8");
  private static final UUID kUuidRpcInbox = UUID.fromString("510c87c8-eb90-11e8-b3dc-17292c2ecc2d");
  private static final UUID kUuidRpcEPoll = UUID.fromString("5140f882-eb90-11e8-a835-13d2bd922d3f");
  private static final byte kRecordDelimiter = (byte) 0x1e; // 30

  private int mNextRequestId = 1;
  private boolean mIsConnected;
  private BluetoothGatt mGatt;
  private BluetoothGattService mGattRpcService;
  private BluetoothGattCharacteristic mGattRpcInbox;
  private BluetoothGattCharacteristic mGattEPoll;
  private ByteBuffer mIncomingData = ByteBuffer.allocate(1024);

  private class OutstandingRequest {
    private Callback mCallback;
    private JSONObject mRequestObject;
    private int mRequestId;

    public OutstandingRequest(int requestId, JSONObject request, Callback callback) {
      mRequestId = requestId;
      mRequestObject = request;
      mCallback = callback;
    }

    public Callback getCallback() {
      return mCallback;
    }

    public JSONObject getRequestObject() {
      return mRequestObject;
    }

    public int getRequestId() {
      return mRequestId;
    }
  }

  private Map<Integer, OutstandingRequest> mOutstandingRequests = new HashMap<Integer, OutstandingRequest>();

  public interface Callback {
    void onResponse(JSONObject request, JSONObject response);
  }

  private JsonRpcClient() {
  }

  public static JsonRpcClient createAndConnect(Context context, BluetoothDevice device) {
    JsonRpcClient client = new JsonRpcClient();
    Log.d(TAG, "connecting to:" + device.getAddress());
    client.mGatt = device.connectGatt(context,false, client);
    return client;
  }

  public int sendRequest(String methodName, JSONObject params, Callback callback) throws JSONException {
    int requestId = mNextRequestId++;

    JSONObject request = new JSONObject();
    request.put("jsonrpc", "2.0");
    request.put("id", requestId);
    request.put("method", methodName);
    request.put("params", params);

    OutstandingRequest outstandingRequest = new OutstandingRequest(requestId, request, callback);
    mOutstandingRequests.put(requestId, outstandingRequest);

    String s = request.toString();
    ByteBuffer buff = ByteBuffer.allocate(256);
    buff.put(StandardCharsets.UTF_8.encode(s));
    buff.flip();

    int n = buff.limit();

    byte [] arr = new byte[n + 1];
    buff.get(arr, 0, n);
    arr[n] = (byte) 0x1e;

    /*
    for (byte b : arr) {
      Log.d(TAG, "byte:" + b);
    }
     */

    // read api docs. This doesn't actually send over-the-air, it just updates local cache
    mGattRpcInbox.setValue(arr);
    boolean writeStarted = mGatt.writeCharacteristic(mGattRpcInbox); // sends over-the-air
    Log.d(TAG, "writeCharacteristic:" + writeStarted);

    return requestId;
  }

  private void dispatchResponse(ByteBuffer data) {
    try {
      String s = StandardCharsets.UTF_8.decode(data).toString();
      JSONObject response = new JSONObject(s);

      if (!response.has("id")) {
        Log.e(TAG, "Invalid json response, missing id");
        return;
      }

      Integer requestId = response.getInt("id");
      OutstandingRequest outstandingRequest = mOutstandingRequests.get(requestId);

      if (outstandingRequest == null) {
        Log.e(TAG, "No registered response for callback with id:" + requestId);
        return;
      }

      Callback callback = outstandingRequest.getCallback();
      if (callback != null) {
        try {
          callback.onResponse(outstandingRequest.getRequestObject(), response);
        } catch (Exception e) {
          Log.e(TAG, "error dispatching users callback for response", e);
        }
      } else {
        Log.e(TAG, "Registered callback for response with id " + requestId + " is null");
      }

      // TODO: do we support multiple response for single request?
      mOutstandingRequests.remove(requestId);

    } catch (JSONException err) {
      Log.e(TAG, "error parsing response", err);
    }
  }

  @Override
  public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
    Log.d(TAG, "onConnectionStateChange:" + status + " state:" + newState);
    if (newState == BluetoothProfile.STATE_CONNECTED) {
      mIsConnected = true;
      Log.d(TAG, "discovering services");
      gatt.discoverServices();
    }
  }

  @Override
  public void onServicesDiscovered(BluetoothGatt gatt, int status) {
    // just for fun/debugging
    List<BluetoothGattService> services = gatt.getServices();
    for (BluetoothGattService service : services) {
      Log.d(TAG, "service:" + service.getUuid());
      for (BluetoothGattCharacteristic characteristic : service.getCharacteristics()) {
        Log.d(TAG, "   char:" + characteristic.getUuid());
      }
    }

    mGattRpcService = gatt.getService(kUuidRpcService);
    mGattRpcInbox = mGattRpcService.getCharacteristic(kUuidRpcInbox);
    mGattEPoll = mGattRpcService.getCharacteristic(kUuidRpcEPoll);
    // TODO: check to ensure mGattEPoll actually has notify capability (it should)
    mGatt.setCharacteristicNotification(mGattEPoll, true);


    try {
      JSONObject params = new JSONObject();
      params.put("command_name", "test-one");

      JSONObject args = new JSONObject();
      args.put("dir", "/tmp");
      params.put("args", args);

      sendRequest("cmd-exec", params, null);
    } catch (JSONException err) {
      Log.e(TAG, "error crafting request", err);
    }
  }

  @Override
  public void onCharacteristicRead(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
    byte[] buff = characteristic.getValue();
    for (int i = 0; i < buff.length; ++i) {
      if (buff[i] != kRecordDelimiter) {
        mIncomingData.put(buff[i]);
      } else {
        ByteBuffer temp = mIncomingData;
        mIncomingData = ByteBuffer.allocate(1024);

        temp.flip();
        dispatchResponse(temp);
      }
    }

    Log.d(TAG, "onCharacteristicRead:" + characteristic.getUuid() + " status:" + status);
  }

  @Override
  public void onCharacteristicWrite(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
    Log.d(TAG, "onCharacteristicWrite:" + characteristic.getUuid() + " status:" + status);
  }

  @Override
  public void onCharacteristicChanged(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic) {
    Log.d(TAG, "onCharacteristicChanged:" + characteristic.getUuid());
    if (characteristic.getUuid().equals(kUuidRpcEPoll)) {
      gatt.readCharacteristic(mGattRpcInbox);
    }
  }

  public boolean isConnected() {
    return mIsConnected;
  }
}
