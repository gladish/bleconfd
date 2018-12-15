package bleconfd.tc.com.bleconfd_example;

import android.app.Dialog;
import android.content.Context;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import android.support.v4.app.DialogFragment;
import android.support.v7.app.AlertDialog;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;
import android.widget.Toast;


/**Test
 * set wifi dialog class
 */
public class DialogSetWifi extends DialogFragment {


    /**
     * click listener
     */
    public interface DialogListener {
        boolean onConnect(DialogFragment dialog, String ssid, String password);
    }

    DialogListener mListener;

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        mListener = (DialogListener) context;
    }

    @NonNull
    @Override
    public Dialog onCreateDialog(@Nullable Bundle savedInstanceState) {
        AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
        // Get the layout inflater
        LayoutInflater inflater = getActivity().getLayoutInflater();


        View view = inflater.inflate(R.layout.dialog_wifi, null);
        builder.setView(view)
                .setTitle("SET RPI WIFI CONNECTION");

        final EditText ssid = view.findViewById(R.id.wifi_ssid);
        final EditText password = view.findViewById(R.id.wifi_password);


        view.findViewById(R.id.wifi_connect).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (ssid.getText().toString().equals("") || password.getText().toString().equals("")) {
                    Toast.makeText(getActivity(), "SSID and password cannot be empty", Toast.LENGTH_SHORT).show();
                    return;
                }
                boolean r = mListener.onConnect(DialogSetWifi.this,
                        ssid.getText().toString(),
                        password.getText().toString());
                if (r) {
                    DialogSetWifi.this.getDialog().cancel();
                } else {
                    Toast.makeText(getActivity(), "write message to rpc failed.", Toast.LENGTH_SHORT).show();
                }
            }
        });

        view.findViewById(R.id.wifi_cancel).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                DialogSetWifi.this.getDialog().cancel();
            }
        });
        return builder.create();
    }
}
