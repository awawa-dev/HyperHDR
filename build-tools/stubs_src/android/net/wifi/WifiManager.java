package android.net.wifi;

public class WifiManager {
    public static final int WIFI_MODE_FULL_HIGH_PERF = 3;
    public class WifiLock {
        public void acquire() {}
        public void release() {}
        public boolean isHeld() { return false; }
    }
    public WifiLock createWifiLock(int lockType, String tag) { return new WifiLock(); }
}
