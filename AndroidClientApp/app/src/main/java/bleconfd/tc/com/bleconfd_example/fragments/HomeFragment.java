package bleconfd.tc.com.bleconfd_example.fragments;

import android.app.Fragment;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.Spinner;
import android.widget.TextView;

import org.json.JSONObject;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Set;

import bleconfd.tc.com.bleconfd_example.BLE.BLEListener;
import bleconfd.tc.com.bleconfd_example.BLE.BLELogger;
import bleconfd.tc.com.bleconfd_example.BLE.BLEManager;
import bleconfd.tc.com.bleconfd_example.Const;
import bleconfd.tc.com.bleconfd_example.R;

/**
 * home fragment page, used to create connection for BLE
 */
public class HomeFragment extends Fragment {

    BluetoothAdapter mBluetoothAdapter = null;

    private TextView logTxt = null;
    private Spinner spinner = null;
    private Button connectBtn = null;
    private Button disConnectBtn = null;
    private final String TAG = "BLE-EXAMPLE";
    private BluetoothDevice currentDevice = null;
    private BLELogger bleLogger = null;

    public HomeFragment() {
    }

    /**
     * create new home page
     *
     * @param position the drawer position
     */
    public static android.app.Fragment newInstance(int position) {
        android.app.Fragment fragment = new HomeFragment();
        Bundle args = new Bundle();
        args.putInt(Const.ARG_PAGE_NUMBER, position);
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        View rootView = inflater.inflate(R.layout.fragment_home, container, false);
        int i = getArguments().getInt(Const.ARG_PAGE_NUMBER);
        String title = getResources().getStringArray(R.array.drawer_item_array)[i];
        getActivity().setTitle(title);

        spinner = rootView.findViewById(R.id.pairedBleSpinner);
        logTxt = rootView.findViewById(R.id.logTxt);
        connectBtn = rootView.findViewById(R.id.connectBtn);
        disConnectBtn = rootView.findViewById(R.id.disConnectBtn);

        mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();

        if (mBluetoothAdapter == null) {
            // Device does not support Bluetooth
            logTxt.setText("Device does not support Bluetooth ...");
        }

        bleLogger = new BLELogger(logTxt, getActivity());
        BLEManager.getInstance().setLogger(bleLogger);
        BLEManager.getInstance().setBleListener(new BLEListener() {
            @Override
            public void onUpdate() {
                updateUI();
            }

            @Override
            public void onClose() {
                updateUI();
            }

            @Override
            public void onMessage(JSONObject message) {
                bleLogger.debug(message.toString());
            }
        });

        initUI();
        return rootView;
    }


    /**
     * init UI
     */
    private void initUI() {
        final ArrayList<String> list = new ArrayList<>();
        list.add("No Devices");

        Set<BluetoothDevice> pairedDevices = new HashSet<>();
        if (mBluetoothAdapter != null) {
            pairedDevices = mBluetoothAdapter.getBondedDevices();
            if (pairedDevices.size() <= 0) {
                logTxt.setText("You need paired rpi3 bluetooth device in android system setting first");
                return;
            }

            for (BluetoothDevice device : pairedDevices) {
                list.add(device.getName());
            }
        }


        ArrayAdapter adapter = new ArrayAdapter(getContext(), R.layout.spinner_item, R.id.spinnerTxt, list);
        spinner.setAdapter(adapter);

        final Set<BluetoothDevice> finalPairedDevices = pairedDevices;
        spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                Log.d(TAG, "selected position = " + position);
                currentDevice = null;
                for (BluetoothDevice device : finalPairedDevices) {
                    if (device.getName().equals(list.get(position))) {
                        currentDevice = device;
                        BLEManager.getInstance().init(getContext(), currentDevice);
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
                BLEManager.getInstance().close();
                bleLogger.clear();
                BLEManager.getInstance().connect();
                updateUI();
            }
        });

        //disconnect button click event
        disConnectBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                BLEManager.getInstance().close();
                bleLogger.clear();
                bleLogger.debug("ble connection closed");
            }
        });


        if (BLEManager.getInstance().getMmDevice() != null) {
            int index = 1;
            for (BluetoothDevice device : pairedDevices) {
                if (device.getAddress().equals(BLEManager.getInstance().getMmDevice().getAddress())) {
                    spinner.setSelection(index);
                    currentDevice = device;
                }
                index += 1;
            }
        }

        if (!BLEManager.getInstance().isClosed() && BLEManager.getInstance().isFoundService()) {
            bleLogger.debug("connection worked as expected");
        }

        this.updateUI();
    }


    /**
     * update buttons status
     */
    private void updateUI() {
        getActivity().runOnUiThread(new Runnable() {
            @Override
            public void run() {

                connectBtn.setEnabled(currentDevice != null && BLEManager.getInstance().isClosed());

                if (!BLEManager.getInstance().isClosed()) {
                    disConnectBtn.setEnabled(true);
                    spinner.setEnabled(false);
                } else {
                    spinner.setEnabled(true);
                    disConnectBtn.setEnabled(false);
                }
            }
        });
    }
}
