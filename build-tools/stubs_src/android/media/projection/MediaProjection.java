package android.media.projection;
import android.hardware.display.VirtualDisplay;
import android.os.Handler;
import android.view.Surface;
public final class MediaProjection {
    public static abstract class Callback {
        public void onStop() {}
    }
    public void registerCallback(Callback callback, Handler handler) {}
    public VirtualDisplay createVirtualDisplay(String name, int width, int height, int dpi, int flags, Surface surface, VirtualDisplay.Callback callback, Handler handler) { return null; }
    public void stop() {}
}
