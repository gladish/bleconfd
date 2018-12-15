package bleconfd.tc.com.bleconfd_example.models;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * the rpc api response
 */
public class ApiResponse {

    private int id = -1;
    private int code = -1;
    private String error = "parse response failed";
    private JSONObject result;

    public ApiResponse(JSONObject res) {

        try {
            id = res.getInt("id");
            code = res.getInt("code");
            if (code != 0) {
                error = res.getJSONObject("error").getString("message");
            } else {
                result = res.getJSONObject("result");
            }
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public int getId() {
        return id;
    }

    public int getCode() {
        return code;
    }

    public String getError() {
        return error;
    }

    public JSONObject getResult() {
        return result;
    }
}
