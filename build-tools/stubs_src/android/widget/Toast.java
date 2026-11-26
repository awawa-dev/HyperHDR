package android.widget;
import android.content.Context;
public class Toast {
    public static final int LENGTH_SHORT = 0;
    public static final int LENGTH_LONG = 1;
    public static Toast makeText(Context context, CharSequence text, int duration) { return new Toast(); }
    public void show() {}
}
