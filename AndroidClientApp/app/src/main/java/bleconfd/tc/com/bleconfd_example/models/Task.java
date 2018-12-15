package bleconfd.tc.com.bleconfd_example.models;

import org.json.JSONObject;

/**
 * rpc request task
 */
public class Task {

    /**
     * task onDone callback
     */
    public interface TaskListener {
        void onDone(ApiResponse response);
    }

    private int requestId;
    private TaskListener listener;

    /**
     * create new task
     *
     * @param requestId the request id
     * @param listener  the callback listener
     */
    public Task(int requestId, TaskListener listener) {
        this.requestId = requestId;
        this.listener = listener;
    }

    /**
     * check returned response the belongs to this task
     *
     * @param object the json object from BLE server
     * @return the result
     */
    public boolean isSameTask(JSONObject object) {
        try {
            if (object.getInt("id") == requestId) {
                return true;
            }
        } catch (Exception e) {
            return false;
        }
        return false;
    }

    public TaskListener getListener() {
        return listener;
    }

    public void setListener(TaskListener listener) {
        this.listener = listener;
    }


}
