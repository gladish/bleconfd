package bleconfd.tc.com.bleconfd_example.ui;

import android.view.MotionEvent;
import android.view.View;

/**
 * add touch effect for image button
 */
public class TouchEffect implements View.OnTouchListener {

    @Override
    public boolean onTouch(View v, MotionEvent event) {
        if (event.getAction() == MotionEvent.ACTION_DOWN) {
            v.setAlpha(.5f);
        } else if (event.getAction() == MotionEvent.ACTION_UP) {
            v.setAlpha(1f);
        }
        return false;
    }
}
