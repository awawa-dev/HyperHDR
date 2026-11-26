package android.media;

import android.graphics.Rect;
import java.nio.ByteBuffer;

public abstract class Image implements AutoCloseable {
    public static abstract class Plane {
        public abstract int getRowStride();
        public abstract int getPixelStride();
        public abstract ByteBuffer getBuffer();
    }
    public abstract int getWidth();
    public abstract int getHeight();
    public abstract Plane[] getPlanes();
    public Rect getCropRect() {
        return new Rect(0, 0, getWidth(), getHeight());
    }
    public abstract void close();
}
