package bleconfd.tc.com.bleconfd_example.ui;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.ListView;

/**
 * INI key value list view
 * this listView will auto set height to show all items
 */
public class INIKeyValueListView extends ListView {

    public INIKeyValueListView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public INIKeyValueListView(Context context) {
        super(context);
    }

    public INIKeyValueListView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int expandSpec = MeasureSpec.makeMeasureSpec(Integer.MAX_VALUE >> 2,
                MeasureSpec.AT_MOST);
        super.onMeasure(widthMeasureSpec, expandSpec);
    }
}