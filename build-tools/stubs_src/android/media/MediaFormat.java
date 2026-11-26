package android.media;

import java.nio.ByteBuffer;

public final class MediaFormat {
    public static final String MIMETYPE_VIDEO_AVC = "video/avc";
    public static final String KEY_COLOR_FORMAT = "color-format";
    public static final String KEY_BIT_RATE = "bitrate";
    public static final String KEY_FRAME_RATE = "frame-rate";
    public static final String KEY_I_FRAME_INTERVAL = "i-frame-interval";
    public static final String KEY_BITRATE_MODE = "bitrate-mode";
    public static final String KEY_WIDTH = "width";
    public static final String KEY_HEIGHT = "height";

    public static MediaFormat createVideoFormat(String mime, int width, int height) {
        return new MediaFormat();
    }

    public void setInteger(String name, int value) {}
    public void setByteBuffer(String name, ByteBuffer bytes) {}
    public boolean containsKey(String name) { return false; }
    public int getInteger(String name) { return 0; }
    public ByteBuffer getByteBuffer(String name) { return null; }
}
