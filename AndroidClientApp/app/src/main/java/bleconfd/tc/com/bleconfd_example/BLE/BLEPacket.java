package bleconfd.tc.com.bleconfd_example.BLE;


import org.json.JSONException;
import org.json.JSONObject;

import java.nio.ByteBuffer;
import java.util.LinkedList;
import java.util.List;
import java.util.PriorityQueue;
import java.util.Queue;


/**
 * rpc BLE packet class
 */
public class BLEPacket {
    private final byte kRecordDelimiter = (short) 30;

    private List<Byte> byteLinkedList = new LinkedList<>();

    private Queue<JSONObject> messages = new PriorityQueue<>();

    /**
     * pack json message to rpc packet
     * @param message the json message
     * @return the packed bytes
     */
    public byte[] pack(String message) {
        byte[] contentBytes = message.getBytes();
        int totalLength = contentBytes.length + 1;

        ByteBuffer byteBuffer = ByteBuffer.allocateDirect(totalLength);
        byteBuffer.position(0);
        byteBuffer.put(contentBytes);
        byteBuffer.position(contentBytes.length);
        byteBuffer.put(kRecordDelimiter);

        byte[] results = new byte[totalLength];
        byteBuffer.position(0);
        byteBuffer.get(results, 0, totalLength);
        return results;
    }


    private void parseMessage() {
        byte[] buffer = new byte[byteLinkedList.size()];
        for (int i = 0; i < byteLinkedList.size(); i++) {
            buffer[i] = byteLinkedList.get(i);
        }
        byteLinkedList = new LinkedList<>();
        String jsonStr = new String(buffer);
        try {
            JSONObject jsonObject = new JSONObject(jsonStr);
            messages.add(jsonObject);
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    /**
     * return the JSON object from packet
     *
     * @param bytes the bytes from rpc
     */
    public void unPack(byte[] bytes) {
        for (int i = 0; i < bytes.length; i++) {
            if (bytes[i] == kRecordDelimiter) {
                parseMessage();
            } else {
                byteLinkedList.add(bytes[i]);
            }
        }
    }

    public Queue<JSONObject> getMessages() {
        return messages;
    }
}
