package org.hyperhdr.androidtv;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.Locale;

public class MainActivity extends Activity {

    private static final int REQ_MEDIA_PROJECTION = 1001;

    private TextView statusText;
    private Button btnCaptureToggle;
    private Button btnGrabberMode;
    private Button btnWledQuick;
    private Button btnTestEffect;
    private Button btnReloadWeb;
    private WebView webView;

    private final Handler uiHandler = new Handler(Looper.getMainLooper());
    private int testModeStep = 0;
    private boolean webLoaded = false;
    private boolean autoCapturePrompted = false;
    private boolean headlessBridgeRequested = false;
    private boolean permissionDialogPending = false;

    private final Runnable statusRefreshRunnable = new Runnable() {
        @Override
        public void run() {
            updateStatusBar();
            uiHandler.postDelayed(this, 1000);
        }
    };

    @SuppressLint("SetJavaScriptEnabled")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        startHyperHdrService();

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(Color.rgb(18, 22, 31));

        // Top Android TV remote-friendly toolbar
        LinearLayout topBar = new LinearLayout(this);
        topBar.setOrientation(LinearLayout.HORIZONTAL);
        topBar.setGravity(Gravity.CENTER_VERTICAL);
        topBar.setPadding(dp(14), dp(8), dp(14), dp(8));
        topBar.setBackgroundColor(Color.rgb(26, 32, 44));

        statusText = new TextView(this);
        statusText.setTextColor(Color.rgb(226, 232, 240));
        statusText.setTextSize(13f);
        statusText.setTypeface(Typeface.MONOSPACE, Typeface.BOLD);
        LinearLayout.LayoutParams statusLp = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        topBar.addView(statusText, statusLp);

        btnCaptureToggle = createTvButton("Start MediaProjection", Color.rgb(16, 185, 129));
        btnCaptureToggle.setOnClickListener(v -> onToggleScreenCapture());
        topBar.addView(btnCaptureToggle, createBtnParams());

        btnGrabberMode = createTvButton("Grabber Mode", Color.rgb(245, 158, 11));
        btnGrabberMode.setOnClickListener(v -> showGrabberModeDialog());
        topBar.addView(btnGrabberMode, createBtnParams());

        btnWledQuick = createTvButton("WLED IP / Scan", Color.rgb(59, 130, 246));
        btnWledQuick.setOnClickListener(v -> showWledQuickDialog());
        topBar.addView(btnWledQuick, createBtnParams());

        btnTestEffect = createTvButton("Test LEDs", Color.rgb(139, 92, 246));
        btnTestEffect.setOnClickListener(v -> onCycleTestEffect());
        topBar.addView(btnTestEffect, createBtnParams());

        btnReloadWeb = createTvButton("Reload UI (:8090)", Color.rgb(71, 85, 105));
        btnReloadWeb.setOnClickListener(v -> {
            if (webView != null) {
                HyperHdrService svc = HyperHdrService.getInstance();
                int port = (svc != null && svc.getServer() != null) ? svc.getServer().getHttpPort() : 8090;
                webView.loadUrl("http://127.0.0.1:" + port + "/");
            }
        });
        topBar.addView(btnReloadWeb, createBtnParams());

        root.addView(topBar, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        // Full embedded HyperHDR v24.0.0-beta.0 Web UI (port 8090)
        webView = new WebView(this);
        WebSettings ws = webView.getSettings();
        ws.setJavaScriptEnabled(true);
        ws.setDomStorageEnabled(true);
        ws.setDatabaseEnabled(true);
        ws.setLoadWithOverviewMode(true);
        ws.setUseWideViewPort(true);
        ws.setBuiltInZoomControls(false);
        ws.setDisplayZoomControls(false);
        ws.setMixedContentMode(WebSettings.MIXED_CONTENT_ALWAYS_ALLOW);
        webView.setBackgroundColor(Color.rgb(24, 28, 36));
        webView.setWebChromeClient(new WebChromeClient());
        webView.setWebViewClient(new WebViewClient());

        root.addView(webView, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1.0f));
        setContentView(root);

        btnCaptureToggle.requestFocus();
        handleIncomingIntent(getIntent());
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        handleIncomingIntent(intent);
    }

    private void handleIncomingIntent(Intent intent) {
        if (intent == null) return;
        String action = intent.getAction();
        if (HyperHdrService.ACTION_REQUEST_MEDIA_PROJECTION.equals(action)
                || intent.getBooleanExtra("auto_projection", false)) {
            headlessBridgeRequested = intent.getBooleanExtra("headless_bridge", false);
            uiHandler.postDelayed(this::requestMediaProjectionConsent, 250);
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        uiHandler.post(statusRefreshRunnable);
    }

    @Override
    protected void onPause() {
        uiHandler.removeCallbacks(statusRefreshRunnable);
        super.onPause();
    }

    private void startHyperHdrService() {
        Intent svc = new Intent(this, HyperHdrService.class);
        svc.setAction(HyperHdrService.ACTION_START_SERVER);
        try {
            // Activity is in foreground, so startService() succeeds without requiring pre-consent mediaProjection FGS
            startService(svc);
        } catch (Throwable t) {
            try {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    startForegroundService(svc);
                }
            } catch (Throwable ignored) {
            }
        }
    }

    private void updateStatusBar() {
        String ip = WledDriver.getLocalIpv4Address();
        HyperHdrService svc = HyperHdrService.getInstance();
        if (svc == null) {
            statusText.setText("Starting HyperHDR v24.0.0-beta.0 Server on " + ip + ":8090 ...");
            startHyperHdrService();
            return;
        }
        if (!webLoaded && webView != null) {
            webLoaded = true;
            webView.loadUrl("http://127.0.0.1:" + svc.getServer().getHttpPort() + "/");
        }
        boolean mpActive = svc.getScreenCaptureManager().isMediaProjectionActive();
        if (!mpActive && !autoCapturePrompted) {
            autoCapturePrompted = true;
            uiHandler.postDelayed(() -> {
                HyperHdrService s = HyperHdrService.getInstance();
                if (s != null && !s.getScreenCaptureManager().isMediaProjectionActive()) {
                    requestMediaProjectionConsent();
                }
            }, 500);
        }
        String wledHost = svc.getWledDriver().getHost();
        double fps = svc.getWledDriver().getCurrentLedFps();
        int leds = svc.getColorEngine().getLedCount();
        int port = svc.getServer().getHttpPort();
        String mode = svc.getScreenCaptureManager().getActiveCaptureMode();

        statusText.setText(String.format(Locale.US,
                "HyperHDR v24.0.0-beta.0  •  Web UI: http://%s:%d  •  Capture: %s  •  WLED: %s (%d LEDs @ %.0f fps)",
                ip, port, mpActive ? ("ACTIVE (" + mode + ")") : "READY (Click Start MediaProjection)", wledHost, leds, fps));

        if (mpActive) {
            btnCaptureToggle.setText("Stop MediaProjection");
            styleButton(btnCaptureToggle, Color.rgb(220, 38, 38), btnCaptureToggle.hasFocus());
        } else {
            btnCaptureToggle.setText("Start MediaProjection");
            styleButton(btnCaptureToggle, Color.rgb(16, 185, 129), btnCaptureToggle.hasFocus());
        }
    }

    public void requestMediaProjectionConsent() {
        if (permissionDialogPending) return;
        HyperHdrService svc = HyperHdrService.getInstance();
        if (svc != null && svc.getScreenCaptureManager().isMediaProjectionActive()) {
            return;
        }
        try {
            MediaProjectionManager mpm = (MediaProjectionManager) getSystemService(Context.MEDIA_PROJECTION_SERVICE);
            if (mpm != null) {
                permissionDialogPending = true;
                startActivityForResult(mpm.createScreenCaptureIntent(), REQ_MEDIA_PROJECTION);
            }
        } catch (Throwable e) {
            permissionDialogPending = false;
            Toast.makeText(this, "MediaProjection error: " + e.getMessage(), Toast.LENGTH_LONG).show();
        }
    }

    private void onToggleScreenCapture() {
        HyperHdrService svc = HyperHdrService.getInstance();
        if (svc != null && svc.getScreenCaptureManager().isMediaProjectionActive()) {
            Intent stopIntent = new Intent(this, HyperHdrService.class);
            stopIntent.setAction(HyperHdrService.ACTION_STOP_CAPTURE);
            startService(stopIntent);
            Toast.makeText(this, "Stopped Android MediaProjection capture", Toast.LENGTH_SHORT).show();
            return;
        }
        requestMediaProjectionConsent();
    }

    private void showGrabberModeDialog() {
        HyperHdrService svc = HyperHdrService.getInstance();
        if (svc == null) return;
        ScreenCaptureManager scm = svc.getScreenCaptureManager();

        final String[] modeLabels = new String[]{
                "Lowest / Lag-Free (64x36 ImageReader) — Recommended for AV1 & VP9",
                "Codec Compatibility (320x180 H.264 MediaCodec)",
                "Codec Compatibility (640x360 H.264 MediaCodec)",
                "Low Overhead (128x72 ImageReader)",
                "Standard (256x144 ImageReader)"
        };

        int currentIdx = 0;
        if (scm.getCaptureMethod() == ScreenCaptureManager.METHOD_CODEC) {
            currentIdx = scm.getCaptureWidth() == 640 ? 2 : 1;
        } else {
            if (scm.getCaptureWidth() == 64) currentIdx = 0;
            else if (scm.getCaptureWidth() == 128) currentIdx = 3;
            else if (scm.getCaptureWidth() == 256) currentIdx = 4;
        }

        new AlertDialog.Builder(this)
                .setTitle("Select Grabber Mode & Resolution")
                .setSingleChoiceItems(modeLabels, currentIdx, (dialog, which) -> {
                    int method = ScreenCaptureManager.METHOD_IMAGE_READER;
                    int w = 64, h = 36;
                    String devName = "Android MediaProjection (Lowest / Lag-Free 64x36)";
                    switch (which) {
                        case 0:
                            method = ScreenCaptureManager.METHOD_IMAGE_READER;
                            w = 64; h = 36;
                            devName = "Android MediaProjection (Lowest / Lag-Free 64x36)";
                            break;
                        case 1:
                            method = ScreenCaptureManager.METHOD_CODEC;
                            w = 320; h = 180;
                            devName = "Android MediaProjection Codec (H.264 Compatibility 320x180)";
                            break;
                        case 2:
                            method = ScreenCaptureManager.METHOD_CODEC;
                            w = 640; h = 360;
                            devName = "Android MediaProjection Codec (H.264 Compatibility 640x360)";
                            break;
                        case 3:
                            method = ScreenCaptureManager.METHOD_IMAGE_READER;
                            w = 128; h = 72;
                            devName = "Android MediaProjection (Low 128x72)";
                            break;
                        case 4:
                            method = ScreenCaptureManager.METHOD_IMAGE_READER;
                            w = 256; h = 144;
                            devName = "Android MediaProjection (Standard 256x144)";
                            break;
                    }
                    scm.setCaptureModeAndResolution(method, w, h);
                    if (svc.getServer() != null) {
                        svc.getServer().saveGrabberDeviceSetting(devName);
                    }
                    dialog.dismiss();
                    Toast.makeText(MainActivity.this, "Active Mode: " + devName, Toast.LENGTH_SHORT).show();
                    updateStatusBar();
                })
                .setNegativeButton("Close", null)
                .show();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQ_MEDIA_PROJECTION) {
            permissionDialogPending = false;
            if (resultCode == Activity.RESULT_OK && data != null) {
                HyperHdrService.setMediaProjectionPermission(resultCode, data);
                HyperHdrService svc = HyperHdrService.getInstance();
                if (svc != null) {
                    svc.startMediaProjectionCapture(resultCode, data);
                } else {
                    Intent capIntent = new Intent(this, HyperHdrService.class);
                    capIntent.setAction(HyperHdrService.ACTION_START_CAPTURE);
                    capIntent.putExtra(HyperHdrService.EXTRA_RESULT_CODE, resultCode);
                    capIntent.putExtra(HyperHdrService.EXTRA_RESULT_DATA, data);
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                        startForegroundService(capIntent);
                    } else {
                        startService(capIntent);
                    }
                }
                updateStatusBar();
                Toast.makeText(this, "Android MediaProjection API Screen Capture Active!", Toast.LENGTH_LONG).show();
                if (headlessBridgeRequested) {
                    headlessBridgeRequested = false;
                    uiHandler.postDelayed(() -> moveTaskToBack(true), 350);
                }
            } else {
                Toast.makeText(this, "MediaProjection screen capture permission was not granted", Toast.LENGTH_SHORT).show();
            }
        }
    }

    private void showWledQuickDialog() {
        HyperHdrService svc = HyperHdrService.getInstance();
        if (svc == null) return;

        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(dp(20), dp(14), dp(20), dp(8));

        TextView label = new TextView(this);
        label.setText("Enter WLED IP address or hostname (or tap 'Scan LAN' to auto-discover WLED controllers):");
        label.setTextSize(14f);
        layout.addView(label);

        final EditText input = new EditText(this);
        input.setInputType(InputType.TYPE_CLASS_TEXT);
        input.setText(svc.getWledDriver().getHost());
        input.setSelection(input.getText().length());
        layout.addView(input);

        AlertDialog dialog = new AlertDialog.Builder(this)
                .setTitle("Configure WLED Controller")
                .setView(layout)
                .setPositiveButton("Save & Connect", (d, which) -> {
                    String newIp = input.getText().toString().trim();
                    if (!newIp.isEmpty()) {
                        svc.updateWledHostQuick(newIp);
                        svc.getWledDriver().identifyDevice(newIp);
                        Toast.makeText(MainActivity.this, "Saved WLED host: " + newIp, Toast.LENGTH_SHORT).show();
                        updateStatusBar();
                    }
                })
                .setNeutralButton("Scan LAN for WLED", (d, which) -> scanLanForWled(svc))
                .setNegativeButton("Cancel", null)
                .create();
        dialog.show();
    }

    private void scanLanForWled(HyperHdrService svc) {
        Toast.makeText(this, "Scanning local network for WLED controllers...", Toast.LENGTH_SHORT).show();
        new Thread(() -> {
            JSONArray found = svc.getWledDriver().discoverDevices();
            uiHandler.post(() -> {
                if (found.length() == 0) {
                    Toast.makeText(MainActivity.this, "No WLED controllers found on subnet", Toast.LENGTH_SHORT).show();
                    return;
                }
                final String[] labels = new String[found.length()];
                final String[] ips = new String[found.length()];
                for (int i = 0; i < found.length(); i++) {
                    JSONObject o = found.optJSONObject(i);
                    labels[i] = o != null ? o.optString("name", "WLED") : "WLED";
                    ips[i] = o != null ? o.optString("value", "") : "";
                }
                new AlertDialog.Builder(MainActivity.this)
                        .setTitle("Select Discovered WLED Controller")
                        .setItems(labels, (d, idx) -> {
                            svc.updateWledHostQuick(ips[idx]);
                            svc.getWledDriver().identifyDevice(ips[idx]);
                            Toast.makeText(MainActivity.this, "Connected to " + labels[idx], Toast.LENGTH_SHORT).show();
                            updateStatusBar();
                        })
                        .show();
            });
        }).start();
    }

    private void onCycleTestEffect() {
        HyperHdrService svc = HyperHdrService.getInstance();
        if (svc == null) return;
        testModeStep = (testModeStep + 1) % 3;
        if (testModeStep == 1) {
            svc.getColorEngine().setEffectPriority(1, "Rainbow swirl fast", -1, "TV Remote");
            Toast.makeText(this, "Test Effect: Rainbow Swirl Fast", Toast.LENGTH_SHORT).show();
        } else if (testModeStep == 2) {
            svc.getColorEngine().setColorPriority(1, new int[]{255, 160, 60}, -1, "TV Remote");
            Toast.makeText(this, "Test Color: Warm Bias Light", Toast.LENGTH_SHORT).show();
        } else {
            svc.getColorEngine().clearAllPriorities();
            Toast.makeText(this, "Cleared override — Live Android MediaProjection Capture active", Toast.LENGTH_SHORT).show();
        }
    }

    private Button createTvButton(String text, final int baseColor) {
        final Button b = new Button(this);
        b.setText(text);
        b.setTextColor(Color.WHITE);
        b.setTextSize(12f);
        b.setAllCaps(false);
        b.setFocusable(true);
        b.setFocusableInTouchMode(false);
        b.setPadding(dp(12), dp(6), dp(12), dp(6));
        styleButton(b, baseColor, false);
        b.setOnFocusChangeListener((v, hasFocus) -> styleButton(b, baseColor, hasFocus));
        return b;
    }

    private void styleButton(Button b, int baseColor, boolean focused) {
        GradientDrawable gd = new GradientDrawable();
        gd.setCornerRadius(dp(6));
        gd.setColor(baseColor);
        if (focused) {
            gd.setStroke(dp(3), Color.rgb(250, 204, 21));
        } else {
            gd.setStroke(dp(1), Color.argb(80, 255, 255, 255));
        }
        b.setBackground(gd);
    }

    private LinearLayout.LayoutParams createBtnParams() {
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
        );
        lp.leftMargin = dp(8);
        return lp;
    }

    private int dp(int v) {
        return Math.round(v * getResources().getDisplayMetrics().density);
    }
}
