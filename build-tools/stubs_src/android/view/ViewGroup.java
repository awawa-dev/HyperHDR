package android.view;
import android.content.Context;
public abstract class ViewGroup extends View {
    public ViewGroup(Context context) { super(context); }
    public void addView(View child) {}
    public void addView(View child, LayoutParams params) {}
    public static class LayoutParams {
        public static final int MATCH_PARENT = -1;
        public static final int WRAP_CONTENT = -2;
        public int width;
        public int height;
        public LayoutParams(int width, int height) {}
    }
    public static class MarginLayoutParams extends LayoutParams {
        public int leftMargin;
        public int topMargin;
        public int rightMargin;
        public int bottomMargin;
        public MarginLayoutParams(int width, int height) { super(width, height); }
    }
}
