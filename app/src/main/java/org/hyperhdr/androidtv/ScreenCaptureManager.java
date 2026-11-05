package org.hyperhdr.androidtv;

import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Shader;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.media.Image;
import android.media.ImageReader;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.DisplayMetrics;

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

public class ScreenCaptureManager {

    public interface FramePreviewListener {
        void onPreviewJpegFrame(byte[] jpegBytes);
    }

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

    private final ExecutorService previewExecutor = Executors.newSingleThreadExecutor();
    private final AtomicBoolean previewEncodingInProgress = new AtomicBoolean(false);

    private final ScheduledExecutorService fallbackScheduler = Executors.newSingleThreadScheduledExecutor();
    private ScheduledFuture<?> fallbackFuture;

    private volatile boolean capturing = false;
    private volatile String activeCaptureMode = "Idle";
    private volatile String lastError = "";
    private volatile boolean imageStreamRequested = false;

    // 256x144 is 16-byte aligned in both width (256=16*16) and height (144=9*16) for Mali/Amlogic/Adreno GPU gralloc
    private volatile int captureWidth = 256;
    private volatile int captureHeight = 144;
    private volatile int targetFps = 30;
    private volatile int cropLeft = 0, cropRight = 0, cropTop = 0, cropBottom = 0;

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

    public ScreenCaptureManager(Context context, InfiniteColorEngine colorEngine) {
        this.context = context.getApplicationContext();
        this.colorEngine = colorEngine;
        try {
            this.bridgeUdpSocket = new DatagramSocket();
        } catch ( Exception ignored ) {
        }
        startFallbackGrabberLoop();
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

    public synchronized void applyConfig(JSONObject rootConfig) {
        if (rootConfig == null) return;
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
            if (dev.contains("320x180")) {
                newW = 320;
                newH = 176; // 16-byte aligned height closest to 180
            } else if (dev.contains("640x360")) {
                newW = 640;
                newH = 368; // 16-byte aligned
            } else {
                newW = 256;
                newH = 144;
            }
        }

        JSONObject vidGrab = rootConfig.optJSONObject("videoGrabber");
        if (vidGrab != null) {
            int vFps = vidGrab.optInt("fps", 0);
            if (vFps >= 5 && vFps <= 60) {
                this.targetFps = vFps;
            }
        }

        if ((newW != this.captureWidth || newH != this.captureHeight)) {
            this.captureWidth = newW;
            this.captureHeight = newH;
            if (capturing && mediaProjection != null) {
                rebuildVirtualDisplay();
            }
        }
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

            captureThread = new HandlerThread("HyperHdrMediaProjection");
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
            activeCaptureMode = "Android MediaProjection API";
            lastError = "";
            windowStartMs = System.currentTimeMillis();
            lastRealFrameAtMs = windowStartMs;
            framesInWindow = 0;

            // Seed an initial frame if latestArgbFrame is still empty so preview/LEDs respond immediately
            if (latestArgbFrame == null) {
                int w = captureWidth;
                int h = captureHeight;
                int[] seed = new int[w * h];
                for (int i = 0; i < seed.length; i++) {
                    seed[i] = 0xFF101824;
                }
                latestArgbFrame = seed;
                latestFrameWidth = w;
                latestFrameHeight = h;
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

            if (virtualDisplay != null) {
                // Android 14+ forbids calling mediaProjection.createVirtualDisplay() multiple times on the same token.
                // Safely resize the existing VirtualDisplay and attach the new surface.
                ImageReader oldReader = this.imageReader;
                ImageReader newReader = ImageReader.newInstance(captureWidth, captureHeight, PixelFormat.RGBA_8888, 4);
                newReader.setOnImageAvailableListener(this::onImageAvailable, captureHandler);
                this.imageReader = newReader;

                virtualDisplay.resize(captureWidth, captureHeight, dpi);
                virtualDisplay.setSurface(newReader.getSurface());

                if (oldReader != null) {
                    try { oldReader.close(); } catch (Throwable ignored) {}
                }
                return true;
            }

            if (imageReader != null) {
                try { imageReader.close(); } catch (Throwable ignored) {}
                imageReader = null;
            }

            imageReader = ImageReader.newInstance(captureWidth, captureHeight, PixelFormat.RGBA_8888, 4);
            imageReader.setOnImageAvailableListener(this::onImageAvailable, captureHandler);

            // Standard AOSP MediaProjection flag is VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR (16)
            int[] flagCandidates = new int[]{
                    DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR,
                    DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR | DisplayManager.VIRTUAL_DISPLAY_FLAG_PUBLIC,
                    DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR | DisplayManager.VIRTUAL_DISPLAY_FLAG_PRESENTATION,
                    0
            };

            Throwable lastEx = null;
            for (int flags : flagCandidates) {
                try {
                    virtualDisplay = mediaProjection.createVirtualDisplay(
                            "HyperHDR-MediaProjection",
                            captureWidth,
                            captureHeight,
                            dpi,
                            flags,
                            imageReader.getSurface(),
                            null,
                            captureHandler
                    );
                    if (virtualDisplay != null) {
                        break;
                    }
                } catch (Throwable ex) {
                    lastEx = ex;
                }
            }

            if (virtualDisplay == null) {
                lastError = lastEx != null
                        ? (lastEx.getClass().getSimpleName() + ": " + lastEx.getMessage())
                        : "createVirtualDisplay returned null";
                return false;
            }
            return true;
        } catch (Throwable t) {
            lastError = t.getClass().getSimpleName() + ": " + t.getMessage();
            return false;
        }
    }

    private synchronized void stopMediaProjectionOnly() {
        capturing = false;
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
                activeCaptureMode = "Android MediaProjection API";
                // If SurfaceFlinger has not pushed a new buffer in >120ms because the TV screen is static,
                // keep feeding the most recent MediaProjection frame so LEDs, FPS telemetry, live preview,
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
            int fmt = bb.getInt();
            if (srcW <= 0 || srcH <= 0 || srcW > 7680 || srcH > 4320) {
                proc.destroy();
                return false;
            }

            int dstW = captureWidth;
            int dstH = captureHeight;
            int[] argb = new int[dstW * dstH];
            int rowBytesLen = srcW * 4;
            byte[] rowBuf = new byte[rowBytesLen];

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
            activeCaptureMode = "Android Screencap";
            latestArgbFrame = argb;
            latestFrameWidth = dstW;
            latestFrameHeight = dstH;
            onDecodedFrame(argb, dstW, dstH, t0);
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
        int[] argb = null;
        int outW = 0;
        int outH = 0;
        long t0 = 0;
        long now = 0;
        try {
            image = reader.acquireLatestImage();
            if (image == null) return;

            now = System.currentTimeMillis();
            long minInterval = Math.max(8L, 1000L / Math.max(5, targetFps));
            // Always decode the very first frame; only throttle subsequent frames if within minInterval
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

            argb = new int[outW * outH];
            byte[] rowBytes = new byte[rowStride];
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
                        argb[rowOff + x] = 0xFF000000 | (r << 16) | (g << 8) | b;
                    }
                }
            }
        } catch (Throwable ignored) {
            argb = null;
        } finally {
            if (image != null) {
                try { image.close(); } catch (Throwable ignored) {}
            }
        }

        if (argb != null && outW > 0 && outH > 0) {
            lastSurfaceFrameAtMs = now;
            latestArgbFrame = argb;
            latestFrameWidth = outW;
            latestFrameHeight = outH;
            activeCaptureMode = "Android MediaProjection API";
            onDecodedFrame(argb, outW, outH, t0);
        }
    }

    private void onDecodedFrame(int[] argb, int w, int h, long t0Ns) {
        long now = System.currentTimeMillis();
        lastFrameProcessedMs = now;
        lastRealFrameAtMs = now;

        colorEngine.updateScreenFrame(argb, w, h);
        publishTermuxBridgeFrame(argb, w, h);

        if ((now - lastPreviewSentMs >= 80 || latestPreviewJpeg == null)
                && previewEncodingInProgress.compareAndSet(false, true)) {
            lastPreviewSentMs = now;
            final int[] frameCopy = argb.clone();
            final int fw = w;
            final int fh = h;
            previewExecutor.execute(() -> {
                try {
                    Bitmap bmp = Bitmap.createBitmap(frameCopy, fw, fh, Bitmap.Config.ARGB_8888);
                    ByteArrayOutputStream baos = new ByteArrayOutputStream(8192);
                    bmp.compress(Bitmap.CompressFormat.JPEG, 75, baos);
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
            // Downscale to 160x90 RGB24 (43,200 bytes + 8-byte header = 43,208 bytes < 64KB UDP limit)
            int bw = 160;
            int bh = 90;
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
            int w = 320, h = 180;
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
                        float x = (ratio / 0.32f) * (w - 16);
                        canvas.drawRect(x, 0, x + 16, 14, p);
                    } else if (ratio < 0.50f) {
                        float y = ((ratio - 0.32f) / 0.18f) * (h - 16);
                        canvas.drawRect(w - 14, y, w, y + 16, p);
                    } else if (ratio < 0.82f) {
                        float x = (1.0f - ((ratio - 0.50f) / 0.32f)) * (w - 16);
                        canvas.drawRect(x, h - 14, x + 16, h, p);
                    } else {
                        float y = (1.0f - ((ratio - 0.82f) / 0.18f)) * (h - 16);
                        canvas.drawRect(0, y, 14, y + 16, p);
                    }
                }
            }

            p.setColor(Color.WHITE);
            p.setTextAlign(Paint.Align.CENTER);
            p.setTextSize(15f);
            canvas.drawText("HyperHDR v24.0.0-beta.0 — Android TV", w / 2f, h / 2f - 10f, p);
            p.setTextSize(12f);
            p.setColor(Color.rgb(180, 220, 255));
            canvas.drawText(isCapturing()
                    ? ("Live Capture Active (" + activeCaptureMode + ")")
                    : "Click 'Start MediaProjection' to Grant Screen Capture", w / 2f, h / 2f + 16f, p);

            ByteArrayOutputStream baos = new ByteArrayOutputStream(8192);
            bmp.compress(Bitmap.CompressFormat.JPEG, 75, baos);
            bmp.recycle();
            return baos.toByteArray();
        } catch (Throwable e) {
            return null;
        }
    }
}
