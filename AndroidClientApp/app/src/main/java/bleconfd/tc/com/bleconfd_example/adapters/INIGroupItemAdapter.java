package bleconfd.tc.com.bleconfd_example.adapters;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ListView;
import android.widget.TextView;

import java.util.List;

import bleconfd.tc.com.bleconfd_example.R;
import bleconfd.tc.com.bleconfd_example.models.INIGroup;
import bleconfd.tc.com.bleconfd_example.ui.TouchEffect;

/**
 * adapter for group list in INI file page
 */
public class INIGroupItemAdapter extends BaseAdapter {

    private Context context;
    private List<INIGroup> groups;
    private OnClickListener mListener;

    public INIGroupItemAdapter(Context context, List<INIGroup> groups, OnClickListener mListener) {
        this.context = context;
        this.groups = groups;
        this.mListener = mListener;
    }

    @Override
    public int getCount() {
        return groups.size();
    }

    @Override
    public INIGroup getItem(int position) {
        return groups.get(position);
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @Override
    public View getView(final int position, View convertView, ViewGroup parent) {
        View view = LayoutInflater.from(context).inflate(R.layout.ini_group_item, null);
        final INIGroup group = getItem(position);
        ImageButton editGroupBtn = view.findViewById(R.id.edit_group_btn);
        editGroupBtn.setOnTouchListener(new TouchEffect());
        editGroupBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mListener.onGroupModifyClick(group.getName());
            }
        });

        ImageButton deleteGroupBtn = view.findViewById(R.id.delete_group_btn);
        deleteGroupBtn.setOnTouchListener(new TouchEffect());
        deleteGroupBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mListener.onGroupDeleteClick(group.getName());
            }
        });

        ((TextView) view.findViewById(R.id.group_name)).setText("[" + group.getName() + "]");

        ListView listView = view.findViewById(R.id.value_listview);
        listView.setAdapter(new INIKeyValueItemAdapter(context, group.getKeyValues(), new INIKeyValueItemAdapter.OnClickListener() {
            @Override
            public void onValueDeleteClick(String key) {
                mListener.onValueDeleteClick(group.getName(), key);
            }

            @Override
            public void onValueModifyClick(String key, String value) {
                mListener.onValueModifyClick(group.getName(), key, value);
            }
        }));
        return view;
    }


    /**
     * Interface for receiving click events from cells.
     */
    public interface OnClickListener {
        void onGroupDeleteClick(String groupName);

        void onGroupModifyClick(String groupName);

        void onValueDeleteClick(String group, String key);

        void onValueModifyClick(String group, String key, String value);
    }
}
