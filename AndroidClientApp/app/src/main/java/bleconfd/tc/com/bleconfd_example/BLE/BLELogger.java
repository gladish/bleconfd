package bleconfd.tc.com.bleconfd_example.BLE;

import android.app.Activity;
import android.widget.TextView;

/**
 * Logger used by BLE Manager
 */
public class BLELogger {
    private TextView view;
    private Activity activity;

    public BLELogger(TextView view, Activity activity) {
        this.view = view;
        this.activity = activity;
    }

    /**
     * debug message
     *
     * @param msg the message value
     */
    public void debug(final String msg) {
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                view.setText(view.getText() + "\n" + msg);
            }
        });
    }

    /**
     * clear logs
     */
    public void clear() {
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                view.setText("");
            }
        });
    }
}
