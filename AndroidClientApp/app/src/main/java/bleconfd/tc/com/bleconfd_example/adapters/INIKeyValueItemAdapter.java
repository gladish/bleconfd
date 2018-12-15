package bleconfd.tc.com.bleconfd_example.adapters;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.ImageButton;
import android.widget.TextView;

import java.util.List;

import bleconfd.tc.com.bleconfd_example.R;
import bleconfd.tc.com.bleconfd_example.models.INIKeyValue;
import bleconfd.tc.com.bleconfd_example.ui.TouchEffect;

/**
 * adapter for value list in INI file page
 */
public class INIKeyValueItemAdapter extends BaseAdapter {

    private Context context;
    private List<INIKeyValue> keyValues;
    private OnClickListener mListener;

    public INIKeyValueItemAdapter(Context context, List<INIKeyValue> keyValues, OnClickListener mListener) {
        this.context = context;
        this.keyValues = keyValues;
        this.mListener = mListener;
    }

    @Override
    public int getCount() {
        return keyValues.size();
    }

    @Override
    public INIKeyValue getItem(int position) {
        return keyValues.get(position);
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @Override
    public View getView(final int position, View convertView, ViewGroup parent) {
        View view = LayoutInflater.from(context).inflate(R.layout.ini_value_item, null);
        final INIKeyValue keyValue = getItem(position);

        ImageButton editBtn = view.findViewById(R.id.edit_btn);
        editBtn.setOnTouchListener(new TouchEffect());
        editBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mListener.onValueModifyClick(keyValue.getKey(), keyValue.getValue());
            }
        });

        ImageButton deleteBtn = view.findViewById(R.id.delete_btn);
        deleteBtn.setOnTouchListener(new TouchEffect());
        deleteBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mListener.onValueDeleteClick(keyValue.getKey());
            }
        });


        ((TextView) view.findViewById(R.id.key_value_txt)).setText(keyValue.getKey() + " = " + keyValue.getValue());
        return view;
    }

    /**
     * Interface for receiving click events from cells.
     */
    public interface OnClickListener {
        void onValueDeleteClick(String key);

        void onValueModifyClick(String key, String value);
    }


}
