package android.os;

public class PowerManager {
    public static final int PARTIAL_WAKE_LOCK = 1;
    public class WakeLock {
        public void acquire() {}
        public void acquire(long timeout) {}
        public void release() {}
        public boolean isHeld() { return false; }
    }
    public WakeLock newWakeLock(int levelAndFlags, String tag) { return new WakeLock(); }
}
