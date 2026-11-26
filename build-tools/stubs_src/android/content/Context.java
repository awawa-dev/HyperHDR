package android.content;
import android.content.res.*;
import java.io.File;
public abstract class Context {
    public static final String ACTIVITY_SERVICE = "activity";
    public static final String MEDIA_PROJECTION_SERVICE = "media_projection";
    public static final String NOTIFICATION_SERVICE = "notification";
    public static final String POWER_SERVICE = "power";
    public static final String WIFI_SERVICE = "wifi";
    public Context getApplicationContext() { return this; }
    public AssetManager getAssets() { return null; }
    public Resources getResources() { return null; }
    public File getFilesDir() { return null; }
    public Object getSystemService(String name) { return null; }
    public void startActivity(Intent intent) {}
    public ComponentName startService(Intent service) { return null; }
    public ComponentName startForegroundService(Intent service) { return null; }
}
