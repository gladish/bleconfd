package bleconfd.tc.com.bleconfd_example.fragments;

import android.app.AlertDialog;
import android.app.Fragment;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

import bleconfd.tc.com.bleconfd_example.BLE.BLEListener;
import bleconfd.tc.com.bleconfd_example.BLE.BLELogger;
import bleconfd.tc.com.bleconfd_example.BLE.BLEManager;
import bleconfd.tc.com.bleconfd_example.BLE.BLEPacket;
import bleconfd.tc.com.bleconfd_example.Const;
import bleconfd.tc.com.bleconfd_example.R;
import bleconfd.tc.com.bleconfd_example.adapters.INIGroupItemAdapter;
import bleconfd.tc.com.bleconfd_example.dialogs.DialogAddOrEditGroup;
import bleconfd.tc.com.bleconfd_example.dialogs.DialogAddOrEditValue;
import bleconfd.tc.com.bleconfd_example.models.ApiResponse;
import bleconfd.tc.com.bleconfd_example.models.INIFile;
import bleconfd.tc.com.bleconfd_example.models.INIGroup;
import bleconfd.tc.com.bleconfd_example.models.Task;

/**
 * ini file page
 */
public class INIFileFragment extends Fragment implements INIGroupItemAdapter.OnClickListener {


    private ListView groupListView;
    private String TAG = "INIFileFragment";

    private Button getAllBtn;
    private Button addGroupBtn;
    private Button addValueBtn;
    private List<Task> tasks;
    private ProgressDialog progressDialog;
    private BLELogger bleLogger;
    private TextView logTextView;
    private BLEPacket blePacket = new BLEPacket();

    public INIFileFragment() {
        tasks = new LinkedList<>();
    }

    /**
     * create new ini file instance
     *
     * @param position the fragment position
     * @return the home fragment
     */
    public static Fragment newInstance(int position) {
        Fragment fragment = new INIFileFragment();
        Bundle args = new Bundle();
        args.putInt(Const.ARG_PAGE_NUMBER, position);
        fragment.setArguments(args);
        return fragment;
    }

    /**
     * create new view
     */
    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        View rootView = inflater.inflate(R.layout.fragment_ini_file, container, false);
        int i = getArguments().getInt(Const.ARG_PAGE_NUMBER);
        String title = getResources().getStringArray(R.array.drawer_item_array)[i];
        getActivity().setTitle(title);

        getAllBtn = rootView.findViewById(R.id.get_all_btn);
        addGroupBtn = rootView.findViewById(R.id.add_group_btn);
        addValueBtn = rootView.findViewById(R.id.add_value_btn);
        groupListView = rootView.findViewById(R.id.listview);
        logTextView = rootView.findViewById(R.id.logTxt);
        bleLogger = new BLELogger(logTextView, getActivity());


        // set empty list
        INIFile iniFile = new INIFile();
        groupListView.setAdapter(new INIGroupItemAdapter(getContext(), iniFile.getGroups(), this));

        // setup BLEManager
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
            public void onMessage(final JSONObject message) {
                Log.d(TAG, "onMessage: " + message.toString());

                // found task
                Task callbackTask = null;
                for (Task task : tasks) {
                    if (task.isSameTask(message)) {
                        callbackTask = task;
                        break;
                    }
                }

                if (callbackTask != null) {
                    // remove it first
                    tasks.remove(callbackTask);

                    final Task finalCallbackTask = callbackTask;

                    // invoke callback
                    getActivity().runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            progressDialog.hide();
                            ApiResponse response = new ApiResponse(message);
                            if (response.getCode() == 0) {
                                finalCallbackTask.getListener().onDone(response);
                            } else {
                                showToast(response.getError());
                            }
                        }
                    });
                }
            }
        });

        progressDialog = ProgressDialog.show(getActivity(), "", "", true);
        progressDialog.hide();

        this.initUI();
        return rootView;
    }

    /**
     * show toast message in UI thread
     *
     * @param message the message
     */
    private void showToast(final String message) {
        getActivity().runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Toast.makeText(getContext(), message, Toast.LENGTH_LONG).show();
            }
        });
    }

    /**
     * init UI elements
     */
    private void initUI() {
        addGroupBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                DialogAddOrEditGroup dialogAddOrEditGroup = new DialogAddOrEditGroup();
                dialogAddOrEditGroup.setArguments(new Bundle());
                dialogAddOrEditGroup.setmListener(new DialogAddOrEditGroup.Listener() {
                    @Override
                    public boolean onAddOrEditGroup(String originName, String newName) {
                        return addOrEditGroup(originName, newName);
                    }
                });
                dialogAddOrEditGroup.show(getFragmentManager(), "addOrEditGroup");
            }
        });

        addValueBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                DialogAddOrEditValue dialogAddOrEditValue = new DialogAddOrEditValue();
                dialogAddOrEditValue.setArguments(new Bundle());
                dialogAddOrEditValue.setmListener(new DialogAddOrEditValue.Listener() {
                    @Override
                    public boolean onAddOrEditValue(String group, String key, String value) {
                        return addOrEditValue(group, key, value);
                    }
                });
                dialogAddOrEditValue.show(getFragmentManager(), "addOrEditValue");
            }
        });

        getAllBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                fetchAllValue();
            }
        });

        this.updateUI();
    }

    /**
     * update ui elements
     */
    private void updateUI() {
        final INIGroupItemAdapter.OnClickListener that = this;
        getActivity().runOnUiThread(new Runnable() {
            @Override
            public void run() {
                boolean canSend = false;
                String msg = "";

                if (BLEManager.getInstance().isClosed()) {
                    msg = "BLE connection didn't create, please connect it in Home page";
                } else if (!BLEManager.getInstance().isFoundService()) {
                    msg = "BLE connected, but service not found.";
                } else {
                    canSend = true;
                }

                if (canSend) {
                    logTextView.setVisibility(View.GONE);
                    getAllBtn.setEnabled(true);
                    addGroupBtn.setEnabled(true);
                    addValueBtn.setEnabled(true);
                } else {
                    logTextView.setVisibility(View.VISIBLE);
                    bleLogger.clear();
                    bleLogger.debug(msg);
                    getAllBtn.setEnabled(false);
                    addGroupBtn.setEnabled(false);
                    addValueBtn.setEnabled(false);
                    groupListView.setAdapter(new INIGroupItemAdapter(getContext(), new LinkedList<INIGroup>(), that));
                }
            }
        });
    }

    /**
     * send request to BLE server
     *
     * @param method   the rpc method
     * @param params   the method params
     * @param listener the returned callback
     */
    private boolean sendRequest(String method, Map<String, String> params, Task.TaskListener listener) {
        try {
            int id = (int) (Math.random() * 100000000);
            JSONObject object = new JSONObject();
            object.put("jsonrpc", "2.0");
            object.put("method", method);
            object.put("id", id);
            JSONObject paramsObj = new JSONObject();
            for (String key : params.keySet()) {
                paramsObj.put(key, params.get(key));
            }
            object.put("params", paramsObj);

            boolean writeResult = BLEManager.getInstance().write(blePacket.pack(object.toString()));
            if (!writeResult) { // write failed
                Toast.makeText(getContext(), "BLE reject this send, maybe it working ? please try again later!", Toast.LENGTH_SHORT).show();
                return false;
            }
            tasks.add(new Task(id, listener));

            progressDialog.setTitle("Invoke BLE API");
            progressDialog.setMessage("invoke " + method + ", send and wait response from server...");
            progressDialog.show();
        } catch (JSONException e) {
            e.printStackTrace();
        }
        return true;
    }

    /**
     * fetch all values
     */
    private void fetchAllValue() {
        final INIGroupItemAdapter.OnClickListener listener = this;

        sendRequest(
                "app-settings-get-all",
                new HashMap<String, String>(), new Task.TaskListener() {
                    @Override
                    public void onDone(ApiResponse response) {
                        try {
                            final INIFile iniFile = INIFile.fromJSON(response.getResult());
                            getActivity().runOnUiThread(new Runnable() {
                                @Override
                                public void run() {
                                    groupListView.setAdapter(new INIGroupItemAdapter(getContext(), iniFile.getGroups(), listener));
                                }
                            });
                        } catch (JSONException e) {
                            e.printStackTrace();
                        }
                    }
                });
    }

    /**
     * invoke add or edit group method
     *
     * @param originName the origin group name
     * @param newName    the new name
     * @return result
     */
    private boolean addOrEditGroup(String originName, String newName) {
        Map<String, String> params = new HashMap<>();
        String method = "app-settings-group-add";
        if (originName == null) { // new group
            params.put("group", newName);
        } else {
            params.put("originGroup", originName);
            params.put("newGroup", newName);
            method = "app-settings-group-modify";
        }
        return sendRequest(method, params, new Task.TaskListener() {
            @Override
            public void onDone(ApiResponse response) {
                fetchAllValue();
            }
        });
    }

    /**
     * invoke add or edit value method
     *
     * @param groupName the group name
     * @param key       the key
     * @param value     the value
     * @return the result
     */
    private boolean addOrEditValue(String groupName, String key, String value) {
        Map<String, String> params = new HashMap<>();

        String method = "app-settings-set";
        params.put("group", groupName);
        params.put("name", key);
        params.put("value", value);

        return sendRequest(method, params, new Task.TaskListener() {
            @Override
            public void onDone(ApiResponse response) {
                fetchAllValue();
            }
        });
    }

    /**
     * popup delete confirm delete dialog
     *
     * @param msg             the delete message
     * @param onClickListener the callback when click YES
     */
    private AlertDialog deleteConfirm(String msg, final DialogInterface.OnClickListener onClickListener) {
        return new AlertDialog.Builder(getContext())
                .setTitle("Delete")
                .setMessage(msg)
                .setPositiveButton("Delete", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {
                        dialog.dismiss();
                        onClickListener.onClick(dialog, whichButton);
                    }

                })
                .setNegativeButton("cancel", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int which) {
                        dialog.dismiss();
                    }
                })
                .create();
    }

    /**
     * on delete group button click
     *
     * @param groupName the group name
     */
    @Override
    public void onGroupDeleteClick(final String groupName) {
        deleteConfirm("Are you sure you want delete group " + groupName + " ?", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                Map<String, String> params = new HashMap<>();
                params.put("group", groupName);
                sendRequest("app-settings-group-delete", params, new Task.TaskListener() {
                    @Override
                    public void onDone(ApiResponse response) {
                        fetchAllValue();
                    }
                });
            }
        }).show();
    }

    /**
     * on edit group name button click
     *
     * @param groupName the group name
     */
    @Override
    public void onGroupModifyClick(final String groupName) {
        DialogAddOrEditGroup dialogAddOrEditGroup = new DialogAddOrEditGroup();
        Bundle bundle = new Bundle();
        bundle.putString("group", groupName);
        dialogAddOrEditGroup.setArguments(bundle);
        dialogAddOrEditGroup.setmListener(new DialogAddOrEditGroup.Listener() {
            @Override
            public boolean onAddOrEditGroup(String originName, String newName) {
                return addOrEditGroup(originName, newName);
            }
        });
        dialogAddOrEditGroup.show(getFragmentManager(), "addOrEditGroup");
    }

    /**
     * on value delete button click
     *
     * @param group the group name
     * @param key   the key name
     */
    @Override
    public void onValueDeleteClick(final String group, final String key) {
        deleteConfirm("Are you sure you want delete this value (key=" + key + ") ?", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                Map<String, String> params = new HashMap<>();
                params.put("group", group);
                params.put("name", key);
                sendRequest("app-settings-delete", params, new Task.TaskListener() {
                    @Override
                    public void onDone(ApiResponse response) {
                        fetchAllValue();
                    }
                });
            }
        }).show();
    }

    /**
     * on value modify click
     *
     * @param group the group name
     * @param key   the key name
     * @param value the value
     */
    @Override
    public void onValueModifyClick(String group, String key, String value) {
        DialogAddOrEditValue dialogAddOrEditValue = new DialogAddOrEditValue();
        Bundle bundle = new Bundle();
        bundle.putString("group", group);
        bundle.putString("key", key);
        bundle.putString("value", value);
        dialogAddOrEditValue.setArguments(bundle);

        dialogAddOrEditValue.setmListener(new DialogAddOrEditValue.Listener() {
            @Override
            public boolean onAddOrEditValue(String group, String key, String value) {
                return addOrEditValue(group, key, value);
            }
        });
        dialogAddOrEditValue.show(getFragmentManager(), "addOrEditValue");
    }
}
