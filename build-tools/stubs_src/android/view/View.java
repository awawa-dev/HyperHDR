package android.view;
import android.content.Context;
import android.graphics.drawable.Drawable;
public class View {
    public interface OnClickListener { void onClick(View v); }
    public interface OnFocusChangeListener { void onFocusChange(View v, boolean hasFocus); }
    public View(Context context) {}
    public void setBackgroundColor(int color) {}
    public void setBackground(Drawable background) {}
    public void setPadding(int left, int top, int right, int bottom) {}
    public void setFocusable(boolean focusable) {}
    public void setFocusableInTouchMode(boolean focusableInTouchMode) {}
    public void setOnClickListener(OnClickListener l) {}
    public void setOnFocusChangeListener(OnFocusChangeListener l) {}
    public boolean requestFocus() { return true; }
    public boolean hasFocus() { return false; }
}
