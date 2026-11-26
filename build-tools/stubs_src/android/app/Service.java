package android.app;
import android.content.*;
import android.os.IBinder;
public abstract class Service extends Context {
    public static final int START_STICKY = 1;
    public void onCreate() {}
    public int onStartCommand(Intent intent, int flags, int startId) { return START_STICKY; }
    public void onDestroy() {}
    public abstract IBinder onBind(Intent intent);
    public final void startForeground(int id, Notification notification) {}
    public final void startForeground(int id, Notification notification, int foregroundServiceType) {}
}
