package org.hyperhdr.androidtv;

import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.Shader;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.media.Image;
import android.media.ImageReader;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Surface;

import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Screen capture manager with dual pipeline support:
 * 1. Standard ImageReader capture with ultra-low resolution (64x36 / 128x72) & zero-allocation buffer reuse.
 *    Drops GPU composition and memory bandwidth by >94%, preventing VP9 1080p and AV1 playback stutter.
 * 2. Hardware MediaCodec H.264 encoder -> decoder pipeline (scrcpy / hyperion-android-reborn technique).
 *    Uses hardware video surfaces directly, bypassing GPU readback and black frame issues on TV SoCs.
 */
public class ScreenCaptureManager {

    private static final String TAG = "HyperHdrScreenCap";

    public interface FramePreviewListener {
        void onPreviewJpegFrame(byte[] jpegBytes);
    }

    public static final int METHOD_IMAGE_READER = 0;
    public static final int METHOD_CODEC = 1;

    private static final int TERMUX_BRIDGE_UDP_PORT = 19446;
    private static final InetAddress LOOPBACK_ADDR;
    static {
        InetAddress addr = null;
        try {
            addr = InetAddress.getByAddress(new byte[]{127, 0, 0, 1});
        } catch (Throwable t) {
            try {
                addr = InetAddress.getByName("127.0.0.1");
            } catch (Throwable ignored) {}
        }
        LOOPBACK_ADDR = addr;
    }

    private final Context context;
    private final InfiniteColorEngine colorEngine;
    private FramePreviewListener previewListener;

    private MediaProjection mediaProjection;
    private VirtualDisplay virtualDisplay;
    private ImageReader imageReader;
    private HandlerThread captureThread;
    private Handler captureHandler;
    private DatagramSocket bridgeUdpSocket;
    private byte[] bridgePacketBuf = null;

    // Codec compatibility pipeline (scrcpy / hyperion-android-reborn trick)
    private MediaCodec codecEncoder;
    private MediaCodec codecDecoder;
    private Surface codecInputSurface;
    private volatile boolean codecDecoderStarted = false;
    private final MediaCodec.BufferInfo encoderBufferInfo = new MediaCodec.BufferInfo();
    private final MediaCodec.BufferInfo decoderBufferInfo = new MediaCodec.BufferInfo();

    private final ExecutorService previewExecutor = Executors.newSingleThreadExecutor();
    private final AtomicBoolean previewEncodingInProgress = new AtomicBoolean(false);

    private final ScheduledExecutorService fallbackScheduler = Executors.newSingleThreadScheduledExecutor();
    private ScheduledFuture<?> fallbackFuture;

    private volatile boolean capturing = false;
    private volatile int captureMethod = METHOD_IMAGE_READER;
    private volatile String activeCaptureMode = "Idle";
    private volatile String lastError = "";
    private volatile boolean imageStreamRequested = false;

    // Default lowest capture resolution (64x36): 16-byte aligned, minimal RAM, zero video stutter
    private volatile int captureWidth = 64;
    private volatile int captureHeight = 36;
    private volatile int targetFps = 30;
    private volatile int cropLeft = 0, cropRight = 0, cropTop = 0, cropBottom = 0;

    // Double-buffered zero-allocation frame buffers
    private final Object bufferLock = new Object();
    private int[] frontArgb = null;
    private int[] backArgb = null;
    private byte[] reusableRowBytes = null;

    private volatile double currentCaptureFps = 0.0;
    private volatile long lastLatencyMs = 2;
    private volatile long lastRealFrameAtMs = 0;
    private volatile long lastSurfaceFrameAtMs = 0;
    private volatile byte[] latestPreviewJpeg = null;
    private volatile int[] latestArgbFrame = null;
    private volatile int latestFrameWidth = 0;
    private volatile int latestFrameHeight = 0;
    private volatile byte[] latestBridgePacket = null;

    private long framesInWindow = 0;
    private long windowStartMs = System.currentTimeMillis();
    private long lastFrameProcessedMs = 0;
    private long lastPreviewSentMs = 0;

    private final Runnable codecStepRunnable = new Runnable() {
        @Override
        public void run() {
            if (!capturing || captureMethod != METHOD_CODEC || captureHandler == null) return;
            try {
                stepCodecPipeline();
            } catch (Throwable t) {
                // Log and continue
            }
            if (capturing && captureMethod == METHOD_CODEC && captureHandler != null) {
                long interval = Math.max(16L, 1000L / Math.max(5, targetFps));
                captureHandler.postDelayed(this, interval);
            }
        }
    };

    public ScreenCaptureManager(Context context, InfiniteColorEngine colorEngine) {
        this.context = context.getApplicationContext();
        this.colorEngine = colorEngine;
        try {
            this.bridgeUdpSocket = new DatagramSocket();
        } catch (Exception ignored) {
        }
        ensureArgbBuffers(captureWidth, captureHeight);
        startFallbackGrabberLoop();
    }

    private void ensureArgbBuffers(int w, int h) {
        synchronized (bufferLock) {
            int len = w * h;
            if (frontArgb == null || frontArgb.length != len) {
                frontArgb = new int[len];
                backArgb = new int[len];
            }
        }
    }

    public void setPreviewListener(FramePreviewListener listener) {
        this.previewListener = listener;
    }

    public void setImageStreamRequested(boolean requested) {
        this.imageStreamRequested = requested;
        if (requested && previewListener != null) {
            byte[] jpeg = latestPreviewJpeg;
            if (jpeg == null) {
                jpeg = generateFallbackPreviewJpeg(null);
            }
            if (jpeg != null) {
                previewListener.onPreviewJpegFrame(jpeg);
            }
        }
    }

    public synchronized void setCaptureModeAndResolution(int method, int width, int height) {
        this.captureMethod = method;
        this.captureWidth = Math.max(16, width & ~1);
        this.captureHeight = Math.max(16, height & ~1);
        ensureArgbBuffers(this.captureWidth, this.captureHeight);
        if (capturing && mediaProjection != null) {
            rebuildVirtualDisplay();
        }
    }

    public synchronized void applyConfig(JSONObject rootConfig) {
        if (rootConfig == null) return;
        int newMethod = this.captureMethod;
        int newW = this.captureWidth;
        int newH = this.captureHeight;

        JSONObject sysGrab = rootConfig.optJSONObject("systemGrabber");
        if (sysGrab != null) {
            int cfgFps = sysGrab.optInt("fps", 30);
            this.targetFps = Math.max(5, Math.min(60, cfgFps));
            this.cropLeft = Math.max(0, sysGrab.optInt("cropLeft", 0));
            this.cropRight = Math.max(0, sysGrab.optInt("cropRight", 0));
            this.cropTop = Math.max(0, sysGrab.optInt("cropTop", 0));
            this.cropBottom = Math.max(0, sysGrab.optInt("cropBottom", 0));

            String dev = sysGrab.optString("device", "");
            if (dev.contains("Codec") || dev.contains("H.264") || dev.contains("compatibility")) {
                newMethod = METHOD_CODEC;
                if (dev.contains("640x360")) {
                    newW = 640;
                    newH = 360;
                } else {
                    newW = 320;
                    newH = 180;
                }
            } else if (dev.contains("320x180")) {
                newMethod = METHOD_IMAGE_READER;
                newW = 320;
                newH = 180;
            } else if (dev.contains("256x144") || dev.contains("Standard")) {
                newMethod = METHOD_IMAGE_READER;
                newW = 256;
                newH = 144;
            } else if (dev.contains("128x72") || dev.contains("Low 128")) {
                newMethod = METHOD_IMAGE_READER;
                newW = 128;
                newH = 72;
            } else if (dev.contains("64x36") || dev.contains("Lowest") || dev.contains("Lag-Free")) {
                newMethod = METHOD_IMAGE_READER;
                newW = 64;
                newH = 36;
            } else {
                // Default: Lowest / Lag-Free (64x36 ImageReader)
                newMethod = METHOD_IMAGE_READER;
                newW = 64;
                newH = 36;
            }
        }

        JSONObject vidGrab = rootConfig.optJSONObject("videoGrabber");
        if (vidGrab != null) {
            int vFps = vidGrab.optInt("fps", 0);
            if (vFps >= 5 && vFps <= 60) {
                this.targetFps = vFps;
            }
        }

        if (newMethod != this.captureMethod || newW != this.captureWidth || newH != this.captureHeight) {
            this.captureMethod = newMethod;
            this.captureWidth = newW;
            this.captureHeight = newH;
            ensureArgbBuffers(this.captureWidth, this.captureHeight);
            if (capturing && mediaProjection != null) {
                rebuildVirtualDisplay();
            }
        }
    }

    public int getCaptureMethod() {
        return captureMethod;
    }

    public int getCaptureWidth() {
        return captureWidth;
    }

    public int getCaptureHeight() {
        return captureHeight;
    }

    public int getTargetFps() {
        return targetFps;
    }

    public String getLastError() {
        return lastError;
    }

    public boolean isMediaProjectionActive() {
        return capturing && mediaProjection != null && virtualDisplay != null;
    }

    public boolean isCapturing() {
        if (capturing && mediaProjection != null) {
            return true;
        }
        return (System.currentTimeMillis() - lastRealFrameAtMs) < 3500L;
    }

    public String getActiveCaptureMode() {
        return activeCaptureMode;
    }

    public double getCurrentCaptureFps() {
        if (isCapturing()) {
            return currentCaptureFps > 0.5 ? currentCaptureFps : targetFps;
        }
        return colorEngine.getSmoothingFps();
    }

    public long getLastLatencyMs() {
        return lastLatencyMs;
    }

    public byte[] getLatestPreviewJpeg() {
        return latestPreviewJpeg;
    }

    public byte[] getLatestBridgePacket() {
        return latestBridgePacket;
    }

    public synchronized boolean startCapture(int resultCode, Intent data) {
        stopMediaProjectionOnly();
        try {
            MediaProjectionManager mpm = (MediaProjectionManager) context.getSystemService(Context.MEDIA_PROJECTION_SERVICE);
            if (mpm == null) {
                lastError = "MediaProjectionManager service unavailable";
                return false;
            }
            if (data == null) {
                lastError = "MediaProjection permission result Intent is null";
                return false;
            }

            mediaProjection = mpm.getMediaProjection(resultCode, data);
            if (mediaProjection == null) {
                lastError = "getMediaProjection returned null (check foregroundServiceType=mediaProjection)";
                return false;
            }

            captureThread = new HandlerThread("HyperHdrScreenCap");
            captureThread.start();
            captureHandler = new Handler(captureThread.getLooper());

            mediaProjection.registerCallback(new MediaProjection.Callback() {
                @Override
                public void onStop() {
                    stopMediaProjectionOnly();
                }
            }, captureHandler);

            if (!rebuildVirtualDisplay()) {
                stopMediaProjectionOnly();
                return false;
            }

            capturing = true;
            lastError = "";
            windowStartMs = System.currentTimeMillis();
            lastRealFrameAtMs = windowStartMs;
            framesInWindow = 0;

            // Seed initial frame so LEDs and preview respond immediately
            ensureArgbBuffers(captureWidth, captureHeight);
            int[] seed = frontArgb;
            if (seed != null) {
                for (int i = 0; i < seed.length; i++) {
                    seed[i] = 0xFF101824;
                }
                latestArgbFrame = seed;
                latestFrameWidth = captureWidth;
                latestFrameHeight = captureHeight;
            }
            return true;
        } catch (Throwable t) {
            lastError = t.getClass().getSimpleName() + ": " + t.getMessage();
            stopMediaProjectionOnly();
            return false;
        }
    }

    private synchronized boolean rebuildVirtualDisplay() {
        try {
            int dpi = 160;
            try {
                DisplayMetrics dm = context.getResources().getDisplayMetrics();
                if (dm != null && dm.densityDpi > 0) {
                    dpi = dm.densityDpi;
                }
            } catch (Throwable ignored) {
            }

            ensureArgbBuffers(captureWidth, captureHeight);

            if (captureMethod == METHOD_CODEC) {
                return setupCodecPipeline(dpi);
            } else {
                return setupImageReaderPipeline(dpi);
            }
        } catch (Throwable t) {
            lastError = t.getClass().getSimpleName() + ": " + t.getMessage();
            // If Codec method fails, fall back to ImageReader lowest resolution
            if (captureMethod == METHOD_CODEC) {
                Log.w(TAG, "MediaCodec pipeline setup failed (" + t.getMessage() + "), falling back to ImageReader");
                captureMethod = METHOD_IMAGE_READER;
                captureWidth = 64;
                captureHeight = 36;
                return setupImageReaderPipeline(160);
            }
            return false;
        }
    }

    private boolean setupImageReaderPipeline(int dpi) {
        releaseCodecResources();

        // 2 gralloc images: 50% less RAM/VRAM footprint than 4 images
        ImageReader newReader = ImageReader.newInstance(captureWidth, captureHeight, PixelFormat.RGBA_8888, 2);
        newReader.setOnImageAvailableListener(this::onImageAvailable, captureHandler);

        ImageReader oldReader = this.imageReader;
        this.imageReader = newReader;

        if (virtualDisplay != null) {
            virtualDisplay.resize(captureWidth, captureHeight, dpi);
            virtualDisplay.setSurface(newReader.getSurface());
            if (oldReader != null) {
                try { oldReader.close(); } catch (Throwable ignored) {}
            }
        } else {
            int[] flagCandidates = new int[]{
                    DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR,
                    DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR | DisplayManager.VIRTUAL_DISPLAY_FLAG_PUBLIC,
                    DisplayManager.VIRTUAL_DISPLAY_FLAG_PUBLIC,
                    0
            };
            Throwable lastEx = null;
            for (int flags : flagCandidates) {
                try {
                    virtualDisplay = mediaProjection.createVirtualDisplay(
                            "HyperHDR-ScreenCap",
                            captureWidth,
                            captureHeight,
                            dpi,
                            flags,
                            imageReader.getSurface(),
                            null,
                            captureHandler
                    );
                    if (virtualDisplay != null) break;
                } catch (Throwable ex) {
                    lastEx = ex;
                }
            }
            if (virtualDisplay == null) {
                lastError = lastEx != null ? (lastEx.getClass().getSimpleName() + ": " + lastEx.getMessage()) : "createVirtualDisplay failed";
                return false;
            }
        }

        activeCaptureMode = "Android MediaProjection (ImageReader " + captureWidth + "x" + captureHeight + ")";
        return true;
    }

    private boolean setupCodecPipeline(int dpi) {
        if (imageReader != null) {
            try { imageReader.close(); } catch (Throwable ignored) {}
            imageReader = null;
        }
        releaseCodecResources();

        try {
            MediaFormat format = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, captureWidth, captureHeight);
            format.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
            format.setInteger(MediaFormat.KEY_BIT_RATE, 1_500_000);
            format.setInteger(MediaFormat.KEY_FRAME_RATE, Math.max(15, targetFps));
            format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 1);
            format.setInteger(MediaFormat.KEY_BITRATE_MODE, MediaCodecInfo.EncoderCapabilities.BITRATE_MODE_CBR);

            codecEncoder = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_VIDEO_AVC);
            codecEncoder.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
            codecInputSurface = codecEncoder.createInputSurface();

            if (virtualDisplay != null) {
                virtualDisplay.resize(captureWidth, captureHeight, dpi);
                virtualDisplay.setSurface(codecInputSurface);
            } else {
                virtualDisplay = mediaProjection.createVirtualDisplay(
                        "HyperHDR-CodecScreen",
                        captureWidth,
                        captureHeight,
                        dpi,
                        DisplayManager.VIRTUAL_DISPLAY_FLAG_PUBLIC,
                        codecInputSurface,
                        null,
                        captureHandler
                );
            }

            codecEncoder.start();
            codecDecoderStarted = false;

            if (captureHandler != null) {
                captureHandler.removeCallbacks(codecStepRunnable);
                captureHandler.post(codecStepRunnable);
            }

            activeCaptureMode = "Android MediaProjection Codec (H.264 " + captureWidth + "x" + captureHeight + ")";
            return true;
        } catch (Throwable t) {
            Log.e(TAG, "Codec pipeline initialization failed", t);
            releaseCodecResources();
            throw new RuntimeException("Codec init error: " + t.getMessage(), t);
        }
    }

    private void stepCodecPipeline() {
        drainCodecEncoder();
        drainCodecDecoder();
    }

    private void drainCodecEncoder() {
        if (codecEncoder == null) return;
        while (true) {
            int index = codecEncoder.dequeueOutputBuffer(encoderBufferInfo, 0);
            if (index == MediaCodec.INFO_TRY_AGAIN_LATER) {
                break;
            } else if (index == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                ensureCodecDecoder(codecEncoder.getOutputFormat());
            } else if (index >= 0) {
                boolean codecConfig = (encoderBufferInfo.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0;
                boolean endOfStream = (encoderBufferInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0;
                if (codecDecoderStarted && !codecConfig && encoderBufferInfo.size > 0) {
                    feedCodecDecoder(index);
                }
                codecEncoder.releaseOutputBuffer(index, false);
                if (endOfStream) break;
            }
        }
    }

    private void ensureCodecDecoder(MediaFormat encoderFormat) {
        if (codecDecoderStarted) return;
        try {
            int w = encoderFormat.containsKey(MediaFormat.KEY_WIDTH) ? encoderFormat.getInteger(MediaFormat.KEY_WIDTH) : captureWidth;
            int h = encoderFormat.containsKey(MediaFormat.KEY_HEIGHT) ? encoderFormat.getInteger(MediaFormat.KEY_HEIGHT) : captureHeight;

            MediaFormat decFormat = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, w, h);
            decFormat.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Flexible);
            decFormat.setInteger(MediaFormat.KEY_FRAME_RATE, Math.max(15, targetFps));

            ByteBuffer csd0 = encoderFormat.getByteBuffer("csd-0");
            if (csd0 != null) decFormat.setByteBuffer("csd-0", csd0);
            if (encoderFormat.containsKey("csd-1")) {
                ByteBuffer csd1 = encoderFormat.getByteBuffer("csd-1");
                if (csd1 != null) decFormat.setByteBuffer("csd-1", csd1);
            }

            codecDecoder = MediaCodec.createDecoderByType(MediaFormat.MIMETYPE_VIDEO_AVC);
            codecDecoder.configure(decFormat, null, null, 0);
            codecDecoder.start();
            codecDecoderStarted = true;
        } catch (Throwable e) {
            Log.e(TAG, "Failed to start H.264 decoder", e);
            codecDecoderStarted = false;
        }
    }

    private void feedCodecDecoder(int encoderIndex) {
        if (codecDecoder == null || !codecDecoderStarted) return;
        int inputIndex = codecDecoder.dequeueInputBuffer(0);
        if (inputIndex < 0) return;

        ByteBuffer encData = codecEncoder.getOutputBuffer(encoderIndex);
        ByteBuffer decInput = codecDecoder.getInputBuffer(inputIndex);
        if (encData != null && decInput != null) {
            decInput.clear();
            if (encoderBufferInfo.size > 0) {
                encData.position(encoderBufferInfo.offset);
                encData.limit(encoderBufferInfo.offset + encoderBufferInfo.size);
                decInput.put(encData);
            }
            codecDecoder.queueInputBuffer(inputIndex, 0, encoderBufferInfo.size,
                    encoderBufferInfo.presentationTimeUs, encoderBufferInfo.flags);
        }
    }

    private void drainCodecDecoder() {
        if (codecDecoder == null || !codecDecoderStarted) return;
        while (true) {
            int index = codecDecoder.dequeueOutputBuffer(decoderBufferInfo, 0);
            if (index == MediaCodec.INFO_TRY_AGAIN_LATER) {
                break;
            } else if (index >= 0) {
                if (decoderBufferInfo.size > 0) {
                    processDecodedCodecImage(index);
                }
                codecDecoder.releaseOutputBuffer(index, false);
                if ((decoderBufferInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) break;
            }
        }
    }

    private void processDecodedCodecImage(int index) {
        Image image = null;
        try {
            image = codecDecoder.getOutputImage(index);
            if (image == null) return;
            long now = System.currentTimeMillis();
            long t0 = System.nanoTime();

            Image.Plane[] planes = image.getPlanes();
            if (planes == null || planes.length < 2) return;

            int dstW = captureWidth;
            int dstH = captureHeight;
            ensureArgbBuffers(dstW, dstH);

            Image.Plane yPlane = planes[0];
            Image.Plane uPlane = planes[1];
            Image.Plane vPlane = planes.length >= 3 ? planes[2] : planes[1];

            ByteBuffer yBuf = yPlane.getBuffer();
            ByteBuffer uBuf = uPlane.getBuffer();
            ByteBuffer vBuf = vPlane.getBuffer();

            int yRowStride = yPlane.getRowStride();
            int yPixelStride = yPlane.getPixelStride();
            int uRowStride = uPlane.getRowStride();
            int uPixelStride = uPlane.getPixelStride();
            int vRowStride = vPlane.getRowStride();
            int vPixelStride = vPlane.getPixelStride();
            boolean semiPlanar = planes.length < 3;

            Rect crop = image.getCropRect();
            int cropL = Math.max(0, crop.left);
            int cropT = Math.max(0, crop.top);
            int cropW = Math.min(image.getWidth() - cropL, crop.width());
            int cropH = Math.min(image.getHeight() - cropT, crop.height());
            if (cropW <= 0 || cropH <= 0) return;

            int[] outPixels = backArgb;
            for (int dy = 0; dy < dstH; dy++) {
                int srcY = cropT + (dy * cropH) / dstH;
                int rowOff = dy * dstW;
                for (int dx = 0; dx < dstW; dx++) {
                    int srcX = cropL + (dx * cropW) / dstW;

                    int yOff = yBuf.position() + srcY * yRowStride + srcX * yPixelStride;
                    int chromaX = srcX / 2;
                    int chromaY = srcY / 2;
                    int uOff = uBuf.position() + chromaY * uRowStride + chromaX * uPixelStride;
                    int vOff = semiPlanar ? uOff + 1 : vBuf.position() + chromaY * vRowStride + chromaX * vPixelStride;

                    int Y = (yBuf.get(yOff) & 0xFF) - 16;
                    int U = (uBuf.get(uOff) & 0xFF) - 128;
                    int V = (vBuf.get(vOff) & 0xFF) - 128;

                    int r = (int) (1.164f * Y + 1.596f * V);
                    int g = (int) (1.164f * Y - 0.392f * U - 0.813f * V);
                    int b = (int) (1.164f * Y + 2.017f * U);

                    int cr = r < 0 ? 0 : (r > 255 ? 255 : r);
                    int cg = g < 0 ? 0 : (g > 255 ? 255 : g);
                    int cb = b < 0 ? 0 : (b > 255 ? 255 : b);

                    outPixels[rowOff + dx] = 0xFF000000 | (cr << 16) | (cg << 8) | cb;
                }
            }

            // Zero-allocation buffer swap
            int[] readyArgb;
            synchronized (bufferLock) {
                int[] tmp = frontArgb;
                frontArgb = backArgb;
                backArgb = tmp;
                readyArgb = frontArgb;
            }

            lastSurfaceFrameAtMs = now;
            latestArgbFrame = readyArgb;
            latestFrameWidth = dstW;
            latestFrameHeight = dstH;
            activeCaptureMode = "Android MediaProjection Codec (H.264 " + dstW + "x" + dstH + ")";
            onDecodedFrame(readyArgb, dstW, dstH, t0);
        } catch (Throwable ignored) {
        } finally {
            if (image != null) {
                try { image.close(); } catch (Throwable ignored) {}
            }
        }
    }

    private void releaseCodecResources() {
        codecDecoderStarted = false;
        if (codecDecoder != null) {
            try { codecDecoder.stop(); } catch (Throwable ignored) {}
            try { codecDecoder.release(); } catch (Throwable ignored) {}
            codecDecoder = null;
        }
        if (codecEncoder != null) {
            try { codecEncoder.stop(); } catch (Throwable ignored) {}
            try { codecEncoder.release(); } catch (Throwable ignored) {}
            codecEncoder = null;
        }
        if (codecInputSurface != null) {
            try { codecInputSurface.release(); } catch (Throwable ignored) {}
            codecInputSurface = null;
        }
    }

    private synchronized void stopMediaProjectionOnly() {
        capturing = false;
        if (captureHandler != null) {
            captureHandler.removeCallbacks(codecStepRunnable);
        }
        releaseCodecResources();
        if (virtualDisplay != null) {
            try { virtualDisplay.release(); } catch (Throwable ignored) {}
            virtualDisplay = null;
        }
        if (imageReader != null) {
            try { imageReader.close(); } catch (Throwable ignored) {}
            imageReader = null;
        }
        if (mediaProjection != null) {
            try { mediaProjection.stop(); } catch (Throwable ignored) {}
            mediaProjection = null;
        }
        if (captureThread != null) {
            try { captureThread.quitSafely(); } catch (Throwable ignored) {}
            captureThread = null;
            captureHandler = null;
        }
    }

    public synchronized void stopCapture() {
        stopMediaProjectionOnly();
        activeCaptureMode = "Idle";
        lastRealFrameAtMs = 0;
        lastSurfaceFrameAtMs = 0;
        latestPreviewJpeg = null;
        latestArgbFrame = null;
        colorEngine.updateScreenFrame(null, 0, 0);
        currentCaptureFps = 0.0;
    }

    public synchronized void stop() {
        stopCapture();
        if (fallbackFuture != null) {
            fallbackFuture.cancel(true);
            fallbackFuture = null;
        }
        fallbackScheduler.shutdownNow();
        previewExecutor.shutdownNow();
        if (bridgeUdpSocket != null && !bridgeUdpSocket.isClosed()) {
            try { bridgeUdpSocket.close(); } catch (Throwable ignored) {}
        }
    }

    private void startFallbackGrabberLoop() {
        if (fallbackFuture != null) {
            fallbackFuture.cancel(false);
        }
        fallbackFuture = fallbackScheduler.scheduleWithFixedDelay(this::pollFallbackTick, 200, 66, TimeUnit.MILLISECONDS);
    }

    private void pollFallbackTick() {
        try {
            if (!colorEngine.getComponentState("ALL")) {
                return;
            }
            boolean sysEnabled = colorEngine.getComponentState("SYSTEMGRABBER") || colorEngine.getComponentState("VIDEOGRABBER");
            if (!colorEngine.getComponentState("GRABBER") || !sysEnabled) {
                return;
            }
            long now = System.currentTimeMillis();

            // When MediaProjection is active:
            if (capturing && mediaProjection != null) {
                // If SurfaceFlinger has not pushed a new buffer in >120ms because the TV screen is static,
                // keep feeding the most recent frame so LEDs, FPS telemetry, live preview,
                // and Termux UDP bridge remain continuously active.
                if (now - lastSurfaceFrameAtMs >= 120L) {
                    int[] cachedArgb = latestArgbFrame;
                    int cw = latestFrameWidth;
                    int ch = latestFrameHeight;
                    if (cachedArgb != null && cw > 0 && ch > 0) {
                        onDecodedFrame(cachedArgb, cw, ch, System.nanoTime());
                    }
                }
                return;
            }

            // If MediaProjection has not been granted yet, try shell screencap fallback
            tryScreencapRawFrame();
        } catch (Throwable ignored) {
        }
    }

    private boolean tryScreencapRawFrame() {
        Process proc = null;
        try {
            long t0 = System.nanoTime();
            proc = new ProcessBuilder("screencap").start();
            InputStream is = proc.getInputStream();

            int hdrLen = Build.VERSION.SDK_INT >= Build.VERSION_CODES.R ? 16 : 12;
            byte[] hdr = readExact(is, hdrLen);
            if (hdr == null) {
                proc.destroy();
                return false;
            }
            ByteBuffer bb = ByteBuffer.wrap(hdr).order(ByteOrder.LITTLE_ENDIAN);
            int srcW = bb.getInt();
            int srcH = bb.getInt();
            if (srcW <= 0 || srcH <= 0 || srcW > 7680 || srcH > 4320) {
                proc.destroy();
                return false;
            }

            int dstW = captureWidth;
            int dstH = captureHeight;
            ensureArgbBuffers(dstW, dstH);
            int[] argb = backArgb;
            int rowBytesLen = srcW * 4;
            if (reusableRowBytes == null || reusableRowBytes.length < rowBytesLen) {
                reusableRowBytes = new byte[rowBytesLen];
            }
            byte[] rowBuf = reusableRowBytes;

            int nextTargetY = 0;
            int dstY = 0;
            for (int sy = 0; sy < srcH; sy++) {
                int r = readUpTo(is, rowBuf, rowBytesLen);
                if (r < rowBytesLen) break;
                if (sy >= nextTargetY && dstY < dstH) {
                    int outRowOff = dstY * dstW;
                    for (int dx = 0; dx < dstW; dx++) {
                        int sx = (dx * srcW) / dstW;
                        int bOff = sx * 4;
                        int red = rowBuf[bOff] & 0xFF;
                        int green = rowBuf[bOff + 1] & 0xFF;
                        int blue = rowBuf[bOff + 2] & 0xFF;
                        argb[outRowOff + dx] = 0xFF000000 | (red << 16) | (green << 8) | blue;
                    }
                    dstY++;
                    nextTargetY = (dstY * srcH) / dstH;
                }
            }
            proc.destroy();
            if (dstY < dstH / 2) {
                return false;
            }

            int[] readyArgb;
            synchronized (bufferLock) {
                int[] tmp = frontArgb;
                frontArgb = backArgb;
                backArgb = tmp;
                readyArgb = frontArgb;
            }

            activeCaptureMode = "Android Screencap";
            latestArgbFrame = readyArgb;
            latestFrameWidth = dstW;
            latestFrameHeight = dstH;
            onDecodedFrame(readyArgb, dstW, dstH, t0);
            return true;
        } catch (Throwable e) {
            if (proc != null) {
                try { proc.destroy(); } catch (Throwable ignored) {}
            }
            return false;
        }
    }

    private static byte[] readExact(InputStream is, int len) throws Exception {
        byte[] buf = new byte[len];
        int off = 0;
        while (off < len) {
            int r = is.read(buf, off, len - off);
            if (r < 0) return null;
            off += r;
        }
        return buf;
    }

    private static int readUpTo(InputStream is, byte[] buf, int len) throws Exception {
        int off = 0;
        while (off < len) {
            int r = is.read(buf, off, len - off);
            if (r < 0) break;
            off += r;
        }
        return off;
    }

    public void injectExternalFrame(int[] argb, int w, int h, String sourceName) {
        if (argb == null || w <= 0 || h <= 0) return;
        activeCaptureMode = sourceName != null ? sourceName : "External Stream";
        latestArgbFrame = argb;
        latestFrameWidth = w;
        latestFrameHeight = h;
        onDecodedFrame(argb, w, h, System.nanoTime());
    }

    private void onImageAvailable(ImageReader reader) {
        Image image = null;
        int outW = 0;
        int outH = 0;
        long t0 = 0;
        long now = 0;
        try {
            image = reader.acquireLatestImage();
            if (image == null) return;

            now = System.currentTimeMillis();
            long minInterval = Math.max(8L, 1000L / Math.max(5, targetFps));
            if (latestArgbFrame != null && (now - lastSurfaceFrameAtMs) < minInterval) {
                return;
            }
            t0 = System.nanoTime();

            Image.Plane[] planes = image.getPlanes();
            if (planes == null || planes.length == 0) return;

            ByteBuffer buffer = planes[0].getBuffer();
            if (buffer == null) return;
            int pixelStride = planes[0].getPixelStride();
            int rowStride = planes[0].getRowStride();
            int w = image.getWidth();
            int h = image.getHeight();
            if (w <= 0 || h <= 0 || pixelStride <= 0 || rowStride <= 0) return;

            int cL = Math.min(w / 3, cropLeft);
            int cR = Math.min(w / 3, cropRight);
            int cT = Math.min(h / 3, cropTop);
            int cB = Math.min(h / 3, cropBottom);
            outW = Math.max(16, w - cL - cR);
            outH = Math.max(16, h - cT - cB);

            ensureArgbBuffers(outW, outH);
            int[] outPixels = backArgb;

            if (reusableRowBytes == null || reusableRowBytes.length < rowStride) {
                reusableRowBytes = new byte[rowStride];
            }
            byte[] rowBytes = reusableRowBytes;
            Buffer baseBuf = (Buffer) buffer;

            for (int y = 0; y < outH; y++) {
                int srcY = y + cT;
                int rowPos = srcY * rowStride;
                if (rowPos >= baseBuf.limit()) break;
                baseBuf.position(rowPos);
                int toRead = Math.min(rowStride, baseBuf.remaining());
                if (toRead <= 0) break;
                buffer.get(rowBytes, 0, toRead);
                int rowOff = y * outW;
                for (int x = 0; x < outW; x++) {
                    int bOff = (x + cL) * pixelStride;
                    if (bOff + 2 < toRead) {
                        int r = rowBytes[bOff] & 0xFF;
                        int g = rowBytes[bOff + 1] & 0xFF;
                        int b = rowBytes[bOff + 2] & 0xFF;
                        outPixels[rowOff + x] = 0xFF000000 | (r << 16) | (g << 8) | b;
                    }
                }
            }

            int[] readyArgb;
            synchronized (bufferLock) {
                int[] tmp = frontArgb;
                frontArgb = backArgb;
                backArgb = tmp;
                readyArgb = frontArgb;
            }

            lastSurfaceFrameAtMs = now;
            latestArgbFrame = readyArgb;
            latestFrameWidth = outW;
            latestFrameHeight = outH;
            activeCaptureMode = "Android MediaProjection (ImageReader " + outW + "x" + outH + ")";
            onDecodedFrame(readyArgb, outW, outH, t0);
        } catch (Throwable ignored) {
        } finally {
            if (image != null) {
                try { image.close(); } catch (Throwable ignored) {}
            }
        }
    }

    private void onDecodedFrame(int[] argb, int w, int h, long t0Ns) {
        long now = System.currentTimeMillis();
        lastFrameProcessedMs = now;
        lastRealFrameAtMs = now;

        colorEngine.updateScreenFrame(argb, w, h);
        publishTermuxBridgeFrame(argb, w, h);

        // Memory-optimized preview encoding:
        // Only encode JPEGs when preview is actively requested (web UI open) or throttled to 2000ms when idle.
        long minPreviewInterval = imageStreamRequested ? 100L : 2000L;
        if ((now - lastPreviewSentMs >= minPreviewInterval || latestPreviewJpeg == null)
                && previewEncodingInProgress.compareAndSet(false, true)) {
            lastPreviewSentMs = now;
            final int[] frameCopy = new int[w * h];
            System.arraycopy(argb, 0, frameCopy, 0, frameCopy.length);
            final int fw = w;
            final int fh = h;
            previewExecutor.execute(() -> {
                try {
                    Bitmap bmp = Bitmap.createBitmap(frameCopy, fw, fh, Bitmap.Config.ARGB_8888);
                    ByteArrayOutputStream baos = new ByteArrayOutputStream(4096);
                    bmp.compress(Bitmap.CompressFormat.JPEG, 70, baos);
                    bmp.recycle();
                    byte[] jpeg = baos.toByteArray();
                    latestPreviewJpeg = jpeg;
                    if (imageStreamRequested && previewListener != null) {
                        previewListener.onPreviewJpegFrame(jpeg);
                    }
                } catch (Throwable ignored) {
                } finally {
                    previewEncodingInProgress.set(false);
                }
            });
        }

        lastLatencyMs = Math.max(1L, (System.nanoTime() - t0Ns) / 1_000_000L);
        framesInWindow++;
        if (now - windowStartMs >= 1000) {
            currentCaptureFps = (framesInWindow * 1000.0) / (now - windowStartMs);
            framesInWindow = 0;
            windowStartMs = now;
        }
    }

    private void publishTermuxBridgeFrame(int[] argb, int w, int h) {
        try {
            if (bridgeUdpSocket == null || LOOPBACK_ADDR == null) return;
            // Downscale to 64x36 or 160x90 RGB24
            int bw = Math.min(160, Math.max(64, w));
            int bh = Math.min(90, Math.max(36, h));
            int totalLen = 8 + bw * bh * 3;
            if (bridgePacketBuf == null || bridgePacketBuf.length != totalLen) {
                bridgePacketBuf = new byte[totalLen];
                bridgePacketBuf[0] = 'H';
                bridgePacketBuf[1] = 'H';
                bridgePacketBuf[2] = 'M';
                bridgePacketBuf[3] = 'P';
                bridgePacketBuf[4] = (byte) ((bw >> 8) & 0xFF);
                bridgePacketBuf[5] = (byte) (bw & 0xFF);
                bridgePacketBuf[6] = (byte) ((bh >> 8) & 0xFF);
                bridgePacketBuf[7] = (byte) (bh & 0xFF);
            }
            byte[] pkt = bridgePacketBuf;
            int idx = 8;
            for (int dy = 0; dy < bh; dy++) {
                int sy = (dy * h) / bh;
                int rowOff = sy * w;
                for (int dx = 0; dx < bw; dx++) {
                    int sx = (dx * w) / bw;
                    int px = argb[rowOff + sx];
                    pkt[idx++] = (byte) ((px >> 16) & 0xFF);
                    pkt[idx++] = (byte) ((px >> 8) & 0xFF);
                    pkt[idx++] = (byte) (px & 0xFF);
                }
            }
            latestBridgePacket = pkt;
            DatagramPacket dp = new DatagramPacket(pkt, totalLen, LOOPBACK_ADDR, TERMUX_BRIDGE_UDP_PORT);
            bridgeUdpSocket.send(dp);
        } catch (Throwable ignored) {
        }
    }

    public byte[] generateFallbackPreviewJpeg(int[] currentLeds) {
        try {
            byte[] cached = latestPreviewJpeg;
            if (cached != null && isCapturing()) {
                return cached;
            }
            int w = 160, h = 90;
            Bitmap bmp = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
            Canvas canvas = new Canvas(bmp);
            Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);

            p.setShader(new LinearGradient(0, 0, w, h,
                    new int[]{Color.rgb(18, 26, 42), Color.rgb(30, 48, 72)},
                    null, Shader.TileMode.CLAMP));
            canvas.drawRect(0, 0, w, h, p);
            p.setShader(null);

            if (currentLeds != null && currentLeds.length >= 12) {
                int ledCount = currentLeds.length / 3;
                for (int i = 0; i < ledCount; i++) {
                    int r = currentLeds[i * 3];
                    int g = currentLeds[i * 3 + 1];
                    int b = currentLeds[i * 3 + 2];
                    p.setColor(Color.rgb(r, g, b));
                    float ratio = (float) i / ledCount;
                    if (ratio < 0.32f) {
                        float x = (ratio / 0.32f) * (w - 8);
                        canvas.drawRect(x, 0, x + 8, 7, p);
                    } else if (ratio < 0.50f) {
                        float y = ((ratio - 0.32f) / 0.18f) * (h - 8);
                        canvas.drawRect(w - 7, y, w, y + 8, p);
                    } else if (ratio < 0.82f) {
                        float x = (1.0f - ((ratio - 0.50f) / 0.32f)) * (w - 8);
                        canvas.drawRect(x, h - 7, x + 8, h, p);
                    } else {
                        float y = (1.0f - ((ratio - 0.82f) / 0.18f)) * (h - 8);
                        canvas.drawRect(0, y, 7, y + 8, p);
                    }
                }
            }

            p.setColor(Color.WHITE);
            p.setTextAlign(Paint.Align.CENTER);
            p.setTextSize(10f);
            canvas.drawText("HyperHDR v24.0.0-beta.0", w / 2f, h / 2f - 4f, p);
            p.setTextSize(8f);
            p.setColor(Color.rgb(180, 220, 255));
            canvas.drawText(isCapturing()
                    ? ("Live (" + activeCaptureMode + ")")
                    : "Ready (Click Start MediaProjection)", w / 2f, h / 2f + 10f, p);

            ByteArrayOutputStream baos = new ByteArrayOutputStream(2048);
            bmp.compress(Bitmap.CompressFormat.JPEG, 70, baos);
            bmp.recycle();
            return baos.toByteArray();
        } catch (Throwable e) {
            return null;
        }
    }
}
