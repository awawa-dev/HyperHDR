package android.widget;
import android.content.Context;
import android.graphics.Typeface;
import android.view.View;
public class TextView extends View {
    public TextView(Context context) { super(context); }
    public void setText(CharSequence text) {}
    public CharSequence getText() { return ""; }
    public void setTextColor(int color) {}
    public void setTextSize(float size) {}
    public void setTypeface(Typeface tf, int style) {}
    public void setAllCaps(boolean allCaps) {}
}
