package bleconfd.tc.com.bleconfd_example.models;

import java.util.List;

/**
 * INI file group object
 */
public class INIGroup {
    private String name;
    private List<INIKeyValue> keyValues;

    public String getName() {
        return name;
    }

    public void setName(String name) {
        this.name = name;
    }

    public List<INIKeyValue> getKeyValues() {
        return keyValues;
    }

    public void setKeyValues(List<INIKeyValue> keyValues) {
        this.keyValues = keyValues;
    }
}
