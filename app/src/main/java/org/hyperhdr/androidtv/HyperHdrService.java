package org.hyperhdr.androidtv;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.IBinder;
import android.os.PowerManager;

import org.json.JSONObject;

public class HyperHdrService extends Service {

    public static final String ACTION_START_SERVER = "org.hyperhdr.androidtv.ACTION_START_SERVER";
    public static final String ACTION_START_CAPTURE = "org.hyperhdr.androidtv.ACTION_START_CAPTURE";
    public static final String ACTION_STOP_CAPTURE = "org.hyperhdr.androidtv.ACTION_STOP_CAPTURE";
    public static final String ACTION_REQUEST_MEDIA_PROJECTION = "org.hyperhdr.androidtv.ACTION_REQUEST_MEDIA_PROJECTION";
    public static final String EXTRA_RESULT_CODE = "result_code";
    public static final String EXTRA_RESULT_DATA = "result_data";

    private static final String CHANNEL_ID = "hyperhdr_server_channel";
    private static final int NOTIF_ID = 2400;

    private static volatile HyperHdrService instance;
    private static volatile int cachedResultCode = 0;
    private static volatile Intent cachedResultData = null;

    private WledDriver wledDriver;
    private InfiniteColorEngine colorEngine;
    private ScreenCaptureManager screenCaptureManager;
    private HyperHdrServer server;
    private PowerManager.WakeLock wakeLock;
    private WifiManager.WifiLock wifiLock;

    public static HyperHdrService getInstance() {
        return instance;
    }

    public static void setMediaProjectionPermission(int resultCode, Intent data) {
        cachedResultCode = resultCode;
        cachedResultData = data;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        instance = this;
        createNotificationChannel();

        try {
            PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
            if (pm != null) {
                wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "HyperHDR:CaptureWakeLock");
                wakeLock.acquire();
            }
            WifiManager wm = (WifiManager) getApplicationContext().getSystemService(Context.WIFI_SERVICE);
            if (wm != null) {
                wifiLock = wm.createWifiLock(WifiManager.WIFI_MODE_FULL_HIGH_PERF, "HyperHDR:WifiLock");
                wifiLock.acquire();
            }
        } catch (Throwable ignored) {
        }

        wledDriver = new WledDriver();
        colorEngine = new InfiniteColorEngine(wledDriver);
        screenCaptureManager = new ScreenCaptureManager(this, colorEngine);
        server = new HyperHdrServer(this, colorEngine, wledDriver, screenCaptureManager);
        server.start();

        // Safely attempt foreground promotion without mediaProjection type before consent is granted
        promoteToForeground(false);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null && intent.getAction() != null) {
            String action = intent.getAction();
            if (ACTION_START_CAPTURE.equals(action)) {
                int resultCode = intent.getIntExtra(EXTRA_RESULT_CODE, cachedResultCode);
                Intent data = intent.getParcelableExtra(EXTRA_RESULT_DATA);
                if (data == null) {
                    data = cachedResultData;
                }
                if (data != null) {
                    startMediaProjectionCapture(resultCode, data);
                } else {
                    server.appendLog("ERROR", "GRABBER", "MediaProjection intent data was null; requesting permission dialog");
                    requestMediaProjectionFromUi();
                }
            } else if (ACTION_STOP_CAPTURE.equals(action)) {
                screenCaptureManager.stopCapture();
                server.appendLog("INFO", "GRABBER", "Android MediaProjection API screen capture stopped");
                promoteToForeground(false);
                if (server != null) {
                    server.notifyGrabberStateChanged();
                }
            } else if (ACTION_REQUEST_MEDIA_PROJECTION.equals(action)) {
                requestMediaProjectionFromUi();
            }
        }
        return START_STICKY;
    }

    public synchronized boolean startMediaProjectionCapture(int resultCode, Intent data) {
        if (data == null) return false;
        cachedResultCode = resultCode;
        cachedResultData = data;

        // Android 10+ requires foreground service with FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION before getMediaProjection()
        promoteToForeground(true);

        if (screenCaptureManager.isMediaProjectionActive()) {
            return true;
        }
        boolean ok = screenCaptureManager.startCapture(resultCode, data);
        if (ok) {
            colorEngine.setComponentState("GRABBER", true);
            colorEngine.setComponentState("SYSTEMGRABBER", true);
            colorEngine.setComponentState("VIDEOGRABBER", true);
            server.appendLog("INFO", "GRABBER",
                    "Android MediaProjection API capture started ("
                            + screenCaptureManager.getCaptureWidth() + "x" + screenCaptureManager.getCaptureHeight()
                            + " @ " + screenCaptureManager.getTargetFps() + "fps RGBA)");
        } else {
            server.appendLog("ERROR", "GRABBER",
                    "Failed to start Android MediaProjection API: " + screenCaptureManager.getLastError());
        }
        promoteToForeground(ok);
        if (server != null) {
            server.notifyGrabberStateChanged();
        }
        return ok;
    }

    public void requestMediaProjectionFromUi() {
        try {
            if (screenCaptureManager != null && screenCaptureManager.isMediaProjectionActive()) {
                return;
            }
            if (cachedResultData != null && cachedResultCode != 0) {
                if (startMediaProjectionCapture(cachedResultCode, cachedResultData)) {
                    return;
                }
            }
            Intent reqIntent = new Intent(this, MainActivity.class);
            reqIntent.setAction(ACTION_REQUEST_MEDIA_PROJECTION);
            reqIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_CLEAR_TOP);
            startActivity(reqIntent);
            if (server != null) {
                server.appendLog("INFO", "GRABBER", "Launched Android MediaProjection API permission prompt on screen");
            }
        } catch (Throwable t) {
            if (server != null) {
                server.appendLog("ERROR", "GRABBER", "Could not launch MediaProjection dialog: " + t.getMessage());
            }
        }
    }

    private void promoteToForeground(boolean withMediaProjectionType) {
        try {
            Notification notification = buildNotification();
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                if (withMediaProjectionType) {
                    try {
                        startForeground(NOTIF_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION);
                        return;
                    } catch (Throwable ignored) {
                    }
                }
            }
            try {
                startForeground(NOTIF_ID, notification);
            } catch (Throwable ignored) {
            }
        } catch (Throwable ignored) {
        }
    }

    private Notification buildNotification() {
        String ip = WledDriver.getLocalIpv4Address();
        int port = server != null ? server.getHttpPort() : 8090;
        boolean cap = screenCaptureManager != null && screenCaptureManager.isCapturing();

        Intent openIntent = new Intent(this, MainActivity.class);
        int piFlags = Build.VERSION.SDK_INT >= Build.VERSION_CODES.M ? PendingIntent.FLAG_IMMUTABLE : 0;
        PendingIntent pi = PendingIntent.getActivity(this, 0, openIntent, piFlags);

        Notification.Builder builder = Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                ? new Notification.Builder(this, CHANNEL_ID)
                : new Notification.Builder(this);

        builder.setContentTitle("HyperHDR v24.0.0-beta.0 Server (" + ip + ":" + port + ")")
                .setContentText((cap ? "MediaProjection: ACTIVE | " : "MediaProjection: Ready | ")
                        + "WLED: " + (wledDriver != null ? wledDriver.getHost() : ""))
                .setSmallIcon(R.drawable.app_icon)
                .setContentIntent(pi)
                .setOngoing(true);

        return builder.build();
    }

    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationManager nm = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
            if (nm != null) {
                NotificationChannel ch = new NotificationChannel(
                        CHANNEL_ID,
                        "HyperHDR v24.0.0-beta.0 Server",
                        NotificationManager.IMPORTANCE_LOW
                );
                ch.setDescription("Runs the HyperHDR v24.0.0-beta.0 Web UI (:8090), Android MediaProjection API Grabber, and WLED driver");
                nm.createNotificationChannel(ch);
            }
        }
    }

    public WledDriver getWledDriver() {
        return wledDriver;
    }

    public InfiniteColorEngine getColorEngine() {
        return colorEngine;
    }

    public ScreenCaptureManager getScreenCaptureManager() {
        return screenCaptureManager;
    }

    public HyperHdrServer getServer() {
        return server;
    }

    public void updateWledHostQuick(String newHost) {
        if (newHost == null || newHost.trim().isEmpty()) return;
        try {
            JSONObject cfg = server.getActiveInstanceConfig();
            JSONObject dev = cfg.optJSONObject("device");
            if (dev == null) {
                dev = new JSONObject();
                cfg.put("device", dev);
            }
            dev.put("type", "wled");
            dev.put("host", newHost.trim());
            server.saveDatabase();
            colorEngine.applyConfig(cfg);
            server.appendLog("INFO", "WLED", "Updated WLED target host to " + newHost.trim());
        } catch (Exception ignored) {
        }
    }

    @Override
    public void onDestroy() {
        if (screenCaptureManager != null) screenCaptureManager.stop();
        if (colorEngine != null) colorEngine.stop();
        if (server != null) server.stop();
        if (wakeLock != null && wakeLock.isHeld()) {
            try { wakeLock.release(); } catch (Throwable ignored) {}
            wakeLock = null;
        }
        if (wifiLock != null && wifiLock.isHeld()) {
            try { wifiLock.release(); } catch (Throwable ignored) {}
            wifiLock = null;
        }
        instance = null;
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
