package bleconfd.tc.com.bleconfd_example.models;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

/**
 * ini file obj
 */
public class INIFile {


    private List<INIGroup> groups = new LinkedList<>();


    public List<INIGroup> getGroups() {
        return groups;
    }

    public void setGroups(List<INIGroup> groups) {
        this.groups = groups;
    }

    /**
     * create ini file obj from json object
     *
     * @param jsonObject the json object from rpc server
     * @return
     * @throws JSONException if error happened
     */
    public static INIFile fromJSON(JSONObject jsonObject) throws JSONException {
        List<INIGroup> iniGroups = new ArrayList<>();

        Iterator<String> groupIt = jsonObject.keys();
        while (groupIt.hasNext()) {
            String groupName = groupIt.next();
            JSONObject keyValuesObj = jsonObject.getJSONObject(groupName);
            Iterator<String> keyIt = keyValuesObj.keys();

            INIGroup iniGroup = new INIGroup();
            iniGroup.setName(groupName);
            List<INIKeyValue> keyValues = new ArrayList<>();

            while (keyIt.hasNext()) {
                String key = keyIt.next();
                INIKeyValue iniKeyValue = new INIKeyValue();
                iniKeyValue.setKey(key);
                iniKeyValue.setValue(keyValuesObj.getString(key));
                keyValues.add(iniKeyValue);
            }
            iniGroup.setKeyValues(keyValues);
            iniGroups.add(iniGroup);
        }

        INIFile iniFile = new INIFile();
        iniFile.setGroups(iniGroups);
        return iniFile;
    }
}
