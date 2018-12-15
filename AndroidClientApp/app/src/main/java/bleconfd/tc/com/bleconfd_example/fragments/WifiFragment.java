package bleconfd.tc.com.bleconfd_example.fragments;

import android.app.DialogFragment;
import android.app.Fragment;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

import org.json.JSONException;
import org.json.JSONObject;

import bleconfd.tc.com.bleconfd_example.BLE.BLEListener;
import bleconfd.tc.com.bleconfd_example.BLE.BLELogger;
import bleconfd.tc.com.bleconfd_example.BLE.BLEManager;
import bleconfd.tc.com.bleconfd_example.BLE.BLEPacket;
import bleconfd.tc.com.bleconfd_example.Const;
import bleconfd.tc.com.bleconfd_example.R;
import bleconfd.tc.com.bleconfd_example.dialogs.DialogSetWifi;

/**
 * set wifi page
 */
public class WifiFragment extends Fragment {

    private Button setWifiButton = null;
    private TextView logTxt = null;
    private BLELogger bleLogger = null;

    public WifiFragment() {
    }

    /**
     * create new set wifi page
     *
     * @param position the drawer position
     */
    public static Fragment newInstance(int position) {
        Fragment fragment = new WifiFragment();
        Bundle args = new Bundle();
        args.putInt(Const.ARG_PAGE_NUMBER, position);
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        View rootView = inflater.inflate(R.layout.fragment_wifi, container, false);
        int i = getArguments().getInt(Const.ARG_PAGE_NUMBER);
        String title = getResources().getStringArray(R.array.drawer_item_array)[i];
        getActivity().setTitle(title);

        setWifiButton = rootView.findViewById(R.id.setWifiBtn);
        logTxt = rootView.findViewById(R.id.logTxt);

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

        this.initUI();
        return rootView;
    }

    /**
     * init set wifi UI
     */
    private void initUI() {
        // set wifi button click event
        setWifiButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                DialogSetWifi dialog = new DialogSetWifi();
                dialog.setmListener(dialogListener);
                dialog.show(getFragmentManager(), "SetWifiDialog");
            }
        });
        this.updateUI();
    }

    /**
     * update wifi page ui
     */
    private void updateUI() {
        getActivity().runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (BLEManager.getInstance().isClosed()) {
                    bleLogger.debug("BLE connection didn't create, please connect it in Home page");
                    setWifiButton.setEnabled(false);
                } else if (!BLEManager.getInstance().isFoundService()) {
                    bleLogger.debug("BLE connected, but service not found.");
                    setWifiButton.setEnabled(false);
                } else {
                    setWifiButton.setEnabled(true);
                }
            }
        });
    }


    /**
     * create set wifi json body
     *
     * @param ssid     the ssid
     * @param password the password
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

        BLEPacket blePacket = new BLEPacket();
        return blePacket.pack(wifiObj.toString());
    }

    /**
     * wifi click connect listener
     */
    private DialogSetWifi.DialogListener dialogListener = new DialogSetWifi.DialogListener() {
        @Override
        public boolean onConnect(DialogFragment dialog, String ssid, String password) {
            if (BLEManager.getInstance().isClosed() || !BLEManager.getInstance().isFoundService()) {
                return false;
            }

            try {
                boolean ret = BLEManager.getInstance().write(connectWifiBody(ssid, password));
                if (ret) {
                    bleLogger.debug("rpc(wifi-connect) invoked, waiting response...");
                }
                return ret;
            } catch (JSONException e) {
                e.printStackTrace();
                return false;
            }
        }
    };
}
