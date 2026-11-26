package android.graphics;
import java.io.OutputStream;
public final class Bitmap {
    public enum Config { ARGB_8888 }
    public enum CompressFormat { JPEG, PNG }
    public static Bitmap createBitmap(int width, int height, Config config) { return null; }
    public static Bitmap createBitmap(int[] colors, int width, int height, Config config) { return null; }
    public int getWidth() { return 0; }
    public int getHeight() { return 0; }
    public void getPixels(int[] pixels, int offset, int stride, int x, int y, int width, int height) {}
    public boolean compress(CompressFormat format, int quality, OutputStream stream) { return true; }
    public void recycle() {}
}
