package android.media;

import android.view.Surface;
import java.io.IOException;
import java.nio.ByteBuffer;

public final class MediaCodec {
    public static final int CONFIGURE_FLAG_ENCODE = 1;
    public static final int INFO_TRY_AGAIN_LATER = -1;
    public static final int INFO_OUTPUT_FORMAT_CHANGED = -2;
    public static final int BUFFER_FLAG_CODEC_CONFIG = 2;
    public static final int BUFFER_FLAG_END_OF_STREAM = 4;

    public static final class BufferInfo {
        public int flags;
        public int offset;
        public int size;
        public long presentationTimeUs;
    }

    public static MediaCodec createEncoderByType(String type) throws IOException {
        return new MediaCodec();
    }

    public static MediaCodec createDecoderByType(String type) throws IOException {
        return new MediaCodec();
    }

    public Surface createInputSurface() {
        return new Surface();
    }

    public void configure(MediaFormat format, Surface surface, Object crypto, int flags) {}
    public void start() {}
    public void stop() {}
    public void release() {}

    public int dequeueOutputBuffer(BufferInfo info, long timeoutUs) { return INFO_TRY_AGAIN_LATER; }
    public int dequeueInputBuffer(long timeoutUs) { return -1; }

    public ByteBuffer getOutputBuffer(int index) { return null; }
    public ByteBuffer getInputBuffer(int index) { return null; }
    public MediaFormat getOutputFormat() { return new MediaFormat(); }

    public void queueInputBuffer(int index, int offset, int size, long presentationTimeUs, int flags) {}
    public void releaseOutputBuffer(int index, boolean render) {}
    public Image getOutputImage(int index) { return null; }
}
