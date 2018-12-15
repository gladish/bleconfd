package bleconfd.tc.com.bleconfd_example.dialogs;

import android.app.AlertDialog;
import android.app.Dialog;
import android.app.DialogFragment;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Toast;

import bleconfd.tc.com.bleconfd_example.R;

/**
 * add or edit key value dialog for ini file page
 */
public class DialogAddOrEditValue extends DialogFragment {

    /**
     * click listener
     */
    public interface Listener {
        boolean onAddOrEditValue(String group, String key, String value);
    }

    Listener mListener;


    public void setmListener(Listener mListener) {
        this.mListener = mListener;
    }


    @NonNull
    @Override
    public Dialog onCreateDialog(@Nullable Bundle savedInstanceState) {
        final String groupName = getArguments().getString("group");
        final String keyName = getArguments().getString("key");
        final String value = getArguments().getString("value");

        AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());

        // Get the layout inflater
        LayoutInflater inflater = getActivity().getLayoutInflater();


        View view = inflater.inflate(R.layout.dialog_add_ini_value, null);
        builder.setView(view)
                .setTitle(groupName == null ? "ADD VALUE" : "EDIT VALUE");

        final EditText groupInput = view.findViewById(R.id.group_name);
        final EditText keyInput = view.findViewById(R.id.key);
        final EditText valueInput = view.findViewById(R.id.value);


        // set values
        if (groupName != null) {
            groupInput.setText(groupName);
            groupInput.setEnabled(false);
        } else {
            groupInput.requestFocus();
        }

        if (keyName != null) {
            keyInput.setText(keyName);
            keyInput.setEnabled(false);
        }

        if (value != null) {
            valueInput.setText(value);
            valueInput.requestFocus();
        }

        groupInput.requestFocus();

        Button okButton = view.findViewById(R.id.ok);
        okButton.setText(groupName == null ? "ADD" : "EDIT");
        okButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (groupInput.getText().toString().trim().isEmpty()) {
                    Toast.makeText(getContext(), "group name cannot be empty", Toast.LENGTH_LONG).show();
                    return;
                }
                if (keyInput.getText().toString().trim().isEmpty()) {
                    Toast.makeText(getContext(), "key cannot be empty", Toast.LENGTH_LONG).show();
                    return;
                }
                if (valueInput.getText().toString().trim().isEmpty()) {
                    Toast.makeText(getContext(), "value cannot be empty", Toast.LENGTH_LONG).show();
                    return;
                }
                if (mListener != null) {
                    mListener.onAddOrEditValue(groupInput.getText().toString(),
                            keyInput.getText().toString(), valueInput.getText().toString());
                }
                DialogAddOrEditValue.this.getDialog().cancel();
            }
        });

        view.findViewById(R.id.cancel).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                DialogAddOrEditValue.this.getDialog().cancel();
            }
        });

        return builder.create();
    }
}
