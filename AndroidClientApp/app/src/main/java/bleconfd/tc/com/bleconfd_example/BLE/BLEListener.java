package bleconfd.tc.com.bleconfd_example.BLE;

import org.json.JSONObject;

/**
 * BLE connection event listener
 */
public interface BLEListener {

    /**
     * when connection state update
     */
    void onUpdate();

    /**
     * when connection closed
     */
    void onClose();

    /**
     * when message from BLE server
     *
     * @param message the message object
     */
    void onMessage(JSONObject message);
}
