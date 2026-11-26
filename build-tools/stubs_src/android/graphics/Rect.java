package android.graphics;

public class Rect {
    public int left;
    public int top;
    public int right;
    public int bottom;

    public Rect() {}

    public Rect(int left, int top, int right, int bottom) {
        this.left = left;
        this.top = top;
        this.right = right;
        this.bottom = bottom;
    }

    public Rect(Rect r) {
        if (r != null) {
            this.left = r.left;
            this.top = r.top;
            this.right = r.right;
            this.bottom = r.bottom;
        }
    }

    public void set(int left, int top, int right, int bottom) {
        this.left = left;
        this.top = top;
        this.right = right;
        this.bottom = bottom;
    }

    public void set(Rect r) {
        if (r != null) {
            this.left = r.left;
            this.top = r.top;
            this.right = r.right;
            this.bottom = r.bottom;
        }
    }

    public int width() {
        return right - left;
    }

    public int height() {
        return bottom - top;
    }

    public String toShortString() {
        return "[" + left + "," + top + "][" + right + "," + bottom + "]";
    }
}
