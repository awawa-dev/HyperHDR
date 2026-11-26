package org.json;
public class JSONObject {
    public JSONObject() {}
    public JSONObject(String json) throws JSONException {}
    public boolean has(String name) { return false; }
    public Object get(String name) throws JSONException { return null; }
    public Object opt(String name) { return null; }
    public Object remove(String name) { return null; }
    public JSONArray names() { return null; }
    public JSONObject put(String name, boolean value) throws JSONException { return this; }
    public JSONObject put(String name, double value) throws JSONException { return this; }
    public JSONObject put(String name, int value) throws JSONException { return this; }
    public JSONObject put(String name, long value) throws JSONException { return this; }
    public JSONObject put(String name, Object value) throws JSONException { return this; }
    public boolean optBoolean(String name) { return false; }
    public boolean optBoolean(String name, boolean fallback) { return fallback; }
    public double optDouble(String name) { return 0; }
    public double optDouble(String name, double fallback) { return fallback; }
    public int optInt(String name) { return 0; }
    public int optInt(String name, int fallback) { return fallback; }
    public long optLong(String name) { return 0; }
    public long optLong(String name, long fallback) { return fallback; }
    public String optString(String name) { return ""; }
    public String optString(String name, String fallback) { return fallback; }
    public JSONArray optJSONArray(String name) { return null; }
    public JSONObject optJSONObject(String name) { return null; }
    public String toString(int indentSpaces) throws JSONException { return ""; }
}
