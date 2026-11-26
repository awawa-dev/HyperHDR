package org.json;
public class JSONArray {
    public JSONArray() {}
    public JSONArray(String json) throws JSONException {}
    public int length() { return 0; }
    public JSONArray put(boolean value) { return this; }
    public JSONArray put(double value) throws JSONException { return this; }
    public JSONArray put(int value) { return this; }
    public JSONArray put(long value) { return this; }
    public JSONArray put(Object value) { return this; }
    public Object opt(int index) { return null; }
    public boolean optBoolean(int index) { return false; }
    public double optDouble(int index) { return 0; }
    public int optInt(int index) { return 0; }
    public int optInt(int index, int fallback) { return fallback; }
    public String optString(int index) { return ""; }
    public JSONObject optJSONObject(int index) { return null; }
    public String getString(int index) throws JSONException { return ""; }
}
