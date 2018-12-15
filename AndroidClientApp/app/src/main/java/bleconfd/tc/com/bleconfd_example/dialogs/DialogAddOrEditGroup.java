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
 * add or edit group dialog for ini file page
 */
public class DialogAddOrEditGroup extends DialogFragment {

    /**
     * click listener
     */
    public interface Listener {
        boolean onAddOrEditGroup(String originName, String newName);
    }

    Listener mListener;


    public void setmListener(Listener mListener) {
        this.mListener = mListener;
    }

    @NonNull
    @Override
    public Dialog onCreateDialog(@Nullable Bundle savedInstanceState) {
        final String groupName = getArguments().getString("group");

        AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());

        // Get the layout inflater
        LayoutInflater inflater = getActivity().getLayoutInflater();


        View view = inflater.inflate(R.layout.dialog_add_group, null);
        builder.setView(view)
                .setTitle(groupName == null ? "ADD GROUP" : "EDIT GROUP");

        final EditText groupInput = view.findViewById(R.id.group_name);

        // set group name
        if (groupName != null) {
            groupInput.setText(groupName);
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

                if (mListener != null) {
                    mListener.onAddOrEditGroup(groupName, groupInput.getText().toString());
                }
                DialogAddOrEditGroup.this.getDialog().cancel();
            }
        });

        view.findViewById(R.id.cancel).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                DialogAddOrEditGroup.this.getDialog().cancel();
            }
        });

        return builder.create();
    }
}
