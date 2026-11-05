package android.hardware.display;
import android.view.Surface;

public final class VirtualDisplay {
    public static abstract class Callback {}
    public void resize(int width, int height, int densityDpi) {}
    public void setSurface(Surface surface) {}
    public void release() {}
}
