package android.os;
public class Handler {
    public Handler() {}
    public Handler(Looper looper) {}
    public boolean post(Runnable r) { return true; }
    public boolean postDelayed(Runnable r, long delayMillis) { return true; }
    public void removeCallbacks(Runnable r) {}
}
