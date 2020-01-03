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

import androidx.appcompat.app.AppCompatActivity;

import android.bluetooth.BluetoothManager;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.content.Context;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.ListView;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class MainActivity extends AppCompatActivity {

  private static final String TAG = "MainActivity";
  private static final int kDefaultScanningTimeout = 10000;

  private Button mScanButton;
  private DeviceFinder mDeviceFinder;
  private JsonRpcClient mRpcClient;

  private ArrayList<String> mDeviceList = new ArrayList<String>();
  private ArrayAdapter<String> mDeviceListAdapter;
  private Map<String, ScanResult> mDeviceDetails = new HashMap<String, ScanResult>();

  private class DeviceFoundCallback extends ScanCallback {
    @Override
    public void onScanResult(int callbackType, ScanResult scanResult) {
      Log.d(TAG, "dev:" + scanResult.toString());

      String mac = scanResult.getDevice().getAddress();
      if (!mDeviceList.contains(mac)) {
        mDeviceList.add(mac);
        mDeviceDetails.put(mac, scanResult);
        mDeviceListAdapter.notifyDataSetChanged();
      }
    }

    @Override
    public void onScanFailed(int errorCode) {
      Log.i(TAG, "error:" + errorCode);
    }

    @Override
    public void onBatchScanResults(List<ScanResult> results) {
      for (ScanResult result : results) {
        Log.d(TAG, "dev:" + result.toString());
      }
    }
  }

  private DeviceFoundCallback mScanCallback = new DeviceFoundCallback();

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_main);

    try {
      final BluetoothManager bluetoothManager = (BluetoothManager) getSystemService(Context.BLUETOOTH_SERVICE);
      mDeviceFinder = new DeviceFinder(bluetoothManager);

      mScanButton = (Button) findViewById(R.id.bleScanButton);
      mScanButton.setOnClickListener(new View.OnClickListener() {
        @Override
        public void onClick(View v) {
          // TODO: somewhere you have to prompt user to enable Bluetooth if it's disabled
          // you also need to check for BLE hw (should be there)
          // you also need to prompt user for permission to use location, etc.
          // right now user has to manually enable this permission
          /*
          BluetoothAdapter bluetoothAdapter = bluetoothManager.getAdapter();
          if (bluetoothAdapter == null || !bluetoothAdapter.isEnabled()) {
            Intent enableBtIntent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
            startActivityForResult(enableBtIntent, REQUEST_ENABLE_BT);
          }
           */
          mDeviceFinder.findDevices(mScanCallback, kDefaultScanningTimeout);
        }
      });

      ListView listView = (ListView) findViewById(R.id.listView);
      mDeviceListAdapter = new ArrayAdapter<String>(this, android.R.layout.simple_list_item_1, mDeviceList);
      listView.setAdapter(mDeviceListAdapter);
      listView.setOnItemClickListener(new AdapterView.OnItemClickListener() {
        // clicking on a mac address in the ListView will initiate the connect with the device
        @Override
        public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
          String mac = mDeviceList.get(position);
          ScanResult scanResult = mDeviceDetails.get(mac);
          mRpcClient = JsonRpcClient.createAndConnect(getApplicationContext(), scanResult.getDevice());
        }
      });

    } catch (Exception e) {
      Log.e(TAG, "Failed to setup ble scanning", e);
    }
  }
}
