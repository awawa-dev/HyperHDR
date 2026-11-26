package android.media;
import android.os.Handler;
import android.view.Surface;
public class ImageReader implements AutoCloseable {
    public interface OnImageAvailableListener {
        void onImageAvailable(ImageReader reader);
    }
    public static ImageReader newInstance(int width, int height, int format, int maxImages) { return null; }
    public Surface getSurface() { return null; }
    public Image acquireLatestImage() { return null; }
    public void setOnImageAvailableListener(OnImageAvailableListener listener, Handler handler) {}
    public void close() {}
}
