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

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanSettings;
import android.os.Handler;
import android.os.ParcelUuid;
import android.util.Log;

import java.util.ArrayList;
import java.util.List;

public class DeviceFinder {

  private static final String TAG = "RdkDeviceFinder";
  private static final String kEddystoneUuid = "0000feaa-0000-1000-8000-00805f9b34fb";

  private static final byte [] kEddystoneNamespace = {
      (byte) 0x00, (byte) 0xE7, (byte) 0x36, (byte) 0xC8, (byte) 0x80, (byte) 0x7B,
      (byte) 0xF4, (byte) 0x60, (byte) 0xCB, (byte) 0x41, (byte) 0xD1, (byte) 0x45
  };

  private BluetoothManager mManager;
  private BluetoothAdapter mAdapter;
  private BluetoothLeScanner mScanner;
  private Handler mHandler;
  private boolean mScanEnabled;
  private ScanCallback mScanCallback;
  private int mScanPeriod;

  // set this to false to disable filtering. If set to false, it'll show ALL ble
  // advertising devices, not just RDK devices
  private boolean mInstallRdkDeviceFilter = true;

  public DeviceFinder(BluetoothManager bluetoothManager) {
    super();
    mManager = bluetoothManager;
    mAdapter = mManager.getAdapter();
    mScanner = mAdapter.getBluetoothLeScanner();
    mHandler = new Handler();
  }

  public void findDevices(ScanCallback callback, int timeoutMillis) {
    mScanCallback = callback;
    mScanPeriod = timeoutMillis;
    if (mScanPeriod == 0)
      mScanPeriod = 10000;

    // TODO: assert user has granted privilegd. Otherwise, you have to
    // go into the settings (gear thing) and allow location for this app after
    // it's installed
      /*
      if (ContextCompat.checkSelfPermission(thisActivity, Manifest.permission.BLUETOOTH_ADMIN)
          != PackageManager.PERMISSION_GRANTED) {
        // Permission is not granted
      }
       */

    scanForDevices(true);
  }

  private void scanForDevices(boolean enable) {
    if (enable) {
      if (mScanEnabled) {
        Log.d(TAG, "scan already in progress");
        return;
      }

      mHandler.postDelayed(new Runnable() {
        @Override
        public void run() {
          Log.d(TAG, "stopping LE scan");
          mScanner.stopScan(mScanCallback);
          mScanEnabled = false;
        }
      }, mScanPeriod);

      ScanSettings.Builder settings = new ScanSettings.Builder();
      settings.setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY);

      List<ScanFilter> filters = new ArrayList<ScanFilter>();
      if (mInstallRdkDeviceFilter) {
        ScanFilter.Builder filter = new ScanFilter.Builder();
        filter.setServiceData(ParcelUuid.fromString(kEddystoneUuid), kEddystoneNamespace);
        filters.add(filter.build());
      }

      Log.i(TAG, "starting BLE scan");
      mScanner.startScan(filters, settings.build(), mScanCallback);
      mScanEnabled = true;
    } else {
      mScanner.stopScan(mScanCallback);
      mScanEnabled = false;
    }
  }
}