package android.os;
public class HandlerThread extends Thread {
    public HandlerThread(String name) {}
    public Looper getLooper() { return null; }
    public boolean quitSafely() { return true; }
}
