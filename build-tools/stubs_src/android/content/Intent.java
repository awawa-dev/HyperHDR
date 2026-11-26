package android.content;
import android.os.Parcelable;
public class Intent implements Parcelable {
    public static final String ACTION_BOOT_COMPLETED = "android.intent.action.BOOT_COMPLETED";
    public static final int FLAG_ACTIVITY_NEW_TASK = 268435456;
    public static final int FLAG_ACTIVITY_SINGLE_TOP = 536870912;
    public static final int FLAG_ACTIVITY_CLEAR_TOP = 67108864;
    public Intent() {}
    public Intent(String action) {}
    public Intent(Context packageContext, Class<?> cls) {}
    public String getAction() { return null; }
    public Intent setAction(String action) { return this; }
    public Intent setFlags(int flags) { return this; }
    public Intent addFlags(int flags) { return this; }
    public int getIntExtra(String name, int defaultValue) { return 0; }
    public boolean getBooleanExtra(String name, boolean defaultValue) { return defaultValue; }
    public String getStringExtra(String name) { return null; }
    public <T extends Parcelable> T getParcelableExtra(String name) { return null; }
    public Intent putExtra(String name, int value) { return this; }
    public Intent putExtra(String name, boolean value) { return this; }
    public Intent putExtra(String name, String value) { return this; }
    public Intent putExtra(String name, Parcelable value) { return this; }
}
