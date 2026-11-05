package org.hyperhdr.androidtv;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;

public class InfiniteColorEngine {

    public interface OutputListener {
        void onLedFrame(int[] rgbFlat);
    }

    public static class LedZone {
        public double hmin;
        public double hmax;
        public double vmin;
        public double vmax;
        public boolean disabled;
    }

    public static class PriorityEntry {
        public int priority;
        public String componentId; // COLOR, EFFECT, IMAGE, SYSTEMGRABBER, VIDEOGRABBER, FLATBUFSERVER, PROTOSERVER, RAWUDPSERVER
        public String origin;
        public String owner;
        public boolean active;
        public boolean visible;
        public long durationMs;
        public long expiresAtMs;
        public int[] solidRgb; // length 3 if COLOR
        public String effectName;
        public int[] imageRgb;
        public int imageWidth;
        public int imageHeight;
    }

    private final WledDriver wledDriver;
    private OutputListener outputListener;

    // Component toggles matching HyperHDR v24.0.0-beta.0
    private final Map<String, Boolean> components = new ConcurrentHashMap<>();

    // Layout & mapping
    private volatile List<LedZone> ledZones = new ArrayList<>();
    private volatile String mappingType = "multicolor_mean";
    private volatile String videoMode = "2D";
    private volatile int videoModeHdr = 0;

    // Color adjustment parameters (Infinite Color Engine linear sRGB + 7-color calibration)
    private volatile String adjustmentId = "default";
    private volatile int[] adjRed = new int[]{255, 0, 0};
    private volatile int[] adjGreen = new int[]{0, 255, 0};
    private volatile int[] adjBlue = new int[]{0, 0, 255};
    private volatile int[] adjCyan = new int[]{0, 255, 255};
    private volatile int[] adjMagenta = new int[]{255, 0, 255};
    private volatile int[] adjYellow = new int[]{255, 255, 0};
    private volatile int[] adjWhite = new int[]{255, 255, 255};
    private volatile double gammaR = 1.5, gammaG = 1.5, gammaB = 1.5;
    private volatile double backlightThreshold = 0.0039;
    private volatile boolean backlightColored = true;
    private volatile int brightness = 100;
    private volatile int brightnessCompensation = 0;
    private volatile int temperature = 6500;
    private volatile double saturationGain = 1.0;
    private volatile double luminanceGain = 1.0;

    // Smoothing parameters (HyperHDR v24 ICE smoothing + Hybrid Inertia + anti-flicker filter)
    private volatile String smoothingType = "linear";
    private volatile int smoothingTimeMs = 150;
    private volatile double updateFrequency = 30.0;
    private volatile double damping = 0.5;
    private volatile boolean lowLightAntiFlickering = true;

    // Black border detector parameters and state
    private volatile boolean blackborderEnabled = true;
    private volatile int blackborderThreshold = 5;
    private volatile String blackborderMode = "default";
    private volatile int blackborderBlurRemove = 1;
    private volatile int blackborderBorderSwitchCnt = 10;
    private volatile int blackborderUnknownTimeout = 300; // ~10 seconds at 30fps

    private volatile int detectedCropTop = 0, detectedCropBottom = 0, detectedCropLeft = 0, detectedCropRight = 0;
    private int candidateCropY = 0;
    private int candidateCropX = 0;
    private int consistentFramesY = 0;
    private int consistentFramesX = 0;
    private int unknownFramesY = 0;
    private int unknownFramesX = 0;

    // Smoothing state buffers (in linear float RGB 0..255)
    private float[] currentFloatRgb = new float[0];
    private float[] targetFloatRgb = new float[0];
    private float[] velocityFloatRgb = new float[0];
    private int[] lastQuantizedRgb = new int[0];

    // Priority muxer
    private final Map<Integer, PriorityEntry> priorities = new ConcurrentHashMap<>();
    private volatile boolean prioritiesAutoselect = true;
    private volatile int manualSelectedPriority = 240;

    // Latest captured screen frame
    private volatile int[] latestScreenRgba = null;
    private volatile int latestScreenWidth = 0;
    private volatile int latestScreenHeight = 0;

    private final ScheduledExecutorService scheduler = Executors.newSingleThreadScheduledExecutor();
    private ScheduledFuture<?> loopFuture;
    private long lastTickNs = System.nanoTime();
    private double effectPhase = 0.0;

    // Performance stats
    private volatile double smoothingFps = 30.0;
    private volatile long smoothingFramesWindow = 0;
    private volatile long smoothingWindowStart = System.currentTimeMillis();

    public InfiniteColorEngine(WledDriver wledDriver) {
        this.wledDriver = wledDriver;
        components.put("ALL", true);
        components.put("SMOOTHING", true);
        components.put("BLACKBORDER", true);
        components.put("FORWARDER", false);
        components.put("BOBLIGHTSERVER", false);
        components.put("GRABBER", true);
        components.put("SYSTEMGRABBER", true);
        components.put("VIDEOGRABBER", true);
        components.put("FLATBUFSERVER", true);
        components.put("PROTOSERVER", true);
        components.put("RAWUDPSERVER", false);
        components.put("LEDDEVICE", true);
        components.put("HDR", false);

        // Register default video & system grabber priorities (240 & 250)
        PriorityEntry vidGrab = new PriorityEntry();
        vidGrab.priority = 240;
        vidGrab.componentId = "VIDEOGRABBER";
        vidGrab.origin = "System";
        vidGrab.owner = "Android MediaProjection API";
        vidGrab.active = true;
        vidGrab.visible = false;
        vidGrab.durationMs = -1;
        vidGrab.expiresAtMs = -1;
        priorities.put(240, vidGrab);

        PriorityEntry sysGrab = new PriorityEntry();
        sysGrab.priority = 250;
        sysGrab.componentId = "SYSTEMGRABBER";
        sysGrab.origin = "System";
        sysGrab.owner = "Android MediaProjection API";
        sysGrab.active = true;
        sysGrab.visible = true;
        sysGrab.durationMs = -1;
        sysGrab.expiresAtMs = -1;
        priorities.put(250, sysGrab);

        startLoop(30.0);
    }

    public void setOutputListener(OutputListener listener) {
        this.outputListener = listener;
    }

    public synchronized void applyConfig(JSONObject rootConfig) {
        if (rootConfig == null) return;

        // 1. LEDs layout
        JSONArray ledsArr = rootConfig.optJSONArray("leds");
        if (ledsArr != null && ledsArr.length() > 0) {
            List<LedZone> zones = new ArrayList<>(ledsArr.length());
            for (int i = 0; i < ledsArr.length(); i++) {
                JSONObject o = ledsArr.optJSONObject(i);
                if (o == null) continue;
                LedZone z = new LedZone();
                z.hmin = o.optDouble("hmin", 0.0);
                z.hmax = o.optDouble("hmax", 1.0);
                z.vmin = o.optDouble("vmin", 0.0);
                z.vmax = o.optDouble("vmax", 1.0);
                z.disabled = o.optBoolean("disabled", false);
                zones.add(z);
            }
            this.ledZones = zones;
            ensureBufferSize(zones.size());
        }

        // 2. Color & channelAdjustment
        JSONObject colorObj = rootConfig.optJSONObject("color");
        if (colorObj != null) {
            this.mappingType = colorObj.optString("imageToLedMappingType", mappingType);
            JSONArray caArr = colorObj.optJSONArray("channelAdjustment");
            if (caArr != null && caArr.length() > 0) {
                JSONObject ca = caArr.optJSONObject(0);
                if (ca != null) {
                    applyAdjustmentObject(ca);
                }
            }
        }

        // 3. Smoothing
        JSONObject smoothObj = rootConfig.optJSONObject("smoothing");
        if (smoothObj != null) {
            boolean en = smoothObj.optBoolean("enable", true);
            components.put("SMOOTHING", en);
            this.smoothingType = smoothObj.optString("type", "linear");
            this.smoothingTimeMs = Math.max(10, smoothObj.optInt("time_ms", 150));
            double freq = Math.max(5.0, Math.min(120.0, smoothObj.optDouble("updateFrequency", 30.0)));
            this.damping = smoothObj.optDouble("damping", 0.5);
            this.lowLightAntiFlickering = smoothObj.optBoolean("lowLightAntiFlickering",
                    smoothObj.optBoolean("antiFlickeringFilter", true));
            if (Math.abs(freq - this.updateFrequency) > 0.5) {
                this.updateFrequency = freq;
                startLoop(freq);
            }
        }

        // 4. Blackborder detector
        JSONObject bbObj = rootConfig.optJSONObject("blackborderdetector");
        if (bbObj != null) {
            boolean en = bbObj.optBoolean("enable", true);
            components.put("BLACKBORDER", en);
            this.blackborderEnabled = en;
            this.blackborderThreshold = Math.max(1, Math.min(50, bbObj.optInt("threshold", 5)));
            this.blackborderMode = bbObj.optString("mode", "default");
            this.blackborderBlurRemove = Math.max(0, bbObj.optInt("blurRemoveCnt", 1));
            this.blackborderBorderSwitchCnt = Math.max(5, bbObj.optInt("borderFrameCnt", 10));
            this.blackborderUnknownTimeout = Math.max(30, bbObj.optInt("unknownFrameCnt", 300));
        } else {
            components.put("BLACKBORDER", true);
            this.blackborderEnabled = true;
        }

        // 5. System & Video Control
        JSONObject sysCtrl = rootConfig.optJSONObject("systemControl");
        if (sysCtrl != null) {
            boolean sysEn = sysCtrl.optBoolean("systemInstanceEnable", true);
            components.put("SYSTEMGRABBER", sysEn);
            PriorityEntry sg = priorities.get(250);
            if (sg != null) sg.active = sysEn;
        }
        JSONObject vidCtrl = rootConfig.optJSONObject("videoControl");
        if (vidCtrl != null) {
            boolean vidEn = vidCtrl.optBoolean("videoInstanceEnable", true);
            components.put("VIDEOGRABBER", vidEn);
            PriorityEntry vg = priorities.get(240);
            if (vg != null) vg.active = vidEn;
        }
        components.put("GRABBER", Boolean.TRUE.equals(components.get("SYSTEMGRABBER")) || Boolean.TRUE.equals(components.get("VIDEOGRABBER")));
        recomputeVisiblePriority();

        // 6. Background effect
        JSONObject bgEff = rootConfig.optJSONObject("backgroundEffect");
        if (bgEff != null) {
            if (bgEff.optBoolean("enable", false)) {
                String effName = bgEff.optString("effect", "Warm mood blobs");
                setEffectPriority(254, effName, -1, "Background");
            } else {
                priorities.remove(254);
                recomputeVisiblePriority();
            }
        }

        // 7. Device config -> WLED driver
        JSONObject devObj = rootConfig.optJSONObject("device");
        if (devObj != null) {
            wledDriver.configure(devObj);
        }
    }

    public synchronized void applyAdjustmentObject(JSONObject ca) {
        if (ca == null) return;
        this.adjustmentId = ca.optString("id", this.adjustmentId);
        this.adjRed = optRgbArray(ca.optJSONArray("red"), this.adjRed);
        this.adjGreen = optRgbArray(ca.optJSONArray("green"), this.adjGreen);
        this.adjBlue = optRgbArray(ca.optJSONArray("blue"), this.adjBlue);
        this.adjCyan = optRgbArray(ca.optJSONArray("cyan"), this.adjCyan);
        this.adjMagenta = optRgbArray(ca.optJSONArray("magenta"), this.adjMagenta);
        this.adjYellow = optRgbArray(ca.optJSONArray("yellow"), this.adjYellow);
        this.adjWhite = optRgbArray(ca.optJSONArray("white"), this.adjWhite);
        double g = ca.optDouble("gamma", Double.NaN);
        if (!Double.isNaN(g)) {
            this.gammaR = g;
            this.gammaG = g;
            this.gammaB = g;
        }
        this.gammaR = ca.optDouble("gammaRed", this.gammaR);
        this.gammaG = ca.optDouble("gammaGreen", this.gammaG);
        this.gammaB = ca.optDouble("gammaBlue", this.gammaB);
        double bt = ca.optDouble("backlightThreshold", this.backlightThreshold);
        if (!Double.isNaN(bt)) {
            this.backlightThreshold = bt;
        }
        this.backlightColored = ca.optBoolean("backlightColored", this.backlightColored);
        this.brightness = ca.optInt("brightness", this.brightness);
        this.brightnessCompensation = ca.optInt("brightnessCompensation", this.brightnessCompensation);
        this.temperature = ca.optInt("temperature", this.temperature);
        this.saturationGain = ca.optDouble("saturationGain", this.saturationGain);
        this.luminanceGain = ca.optDouble("luminanceGain", this.luminanceGain);
    }

    public JSONArray getAdjustmentJsonArray() {
        JSONArray arr = new JSONArray();
        try {
            JSONObject o = new JSONObject();
            o.put("id", adjustmentId);
            o.put("red", toJsonArray(adjRed));
            o.put("green", toJsonArray(adjGreen));
            o.put("blue", toJsonArray(adjBlue));
            o.put("cyan", toJsonArray(adjCyan));
            o.put("magenta", toJsonArray(adjMagenta));
            o.put("yellow", toJsonArray(adjYellow));
            o.put("white", toJsonArray(adjWhite));
            o.put("black", toJsonArray(new int[]{0, 0, 0}));
            o.put("gammaRed", gammaR);
            o.put("gammaGreen", gammaG);
            o.put("gammaBlue", gammaB);
            o.put("backlightThreshold", backlightThreshold);
            o.put("backlightColored", backlightColored);
            o.put("brightness", brightness);
            o.put("brightnessCompensation", brightnessCompensation);
            o.put("temperature", temperature);
            o.put("saturationGain", saturationGain);
            o.put("luminanceGain", luminanceGain);
            arr.put(o);
        } catch (Exception ignored) {
        }
        return arr;
    }

    private synchronized void ensureBufferSize(int ledCount) {
        int n = Math.max(1, ledCount) * 3;
        if (currentFloatRgb.length != n) {
            currentFloatRgb = new float[n];
            targetFloatRgb = new float[n];
            velocityFloatRgb = new float[n];
            lastQuantizedRgb = new int[n];
        }
    }

    private synchronized void startLoop(double hz) {
        if (loopFuture != null) {
            loopFuture.cancel(false);
        }
        long periodMs = Math.max(8L, Math.round(1000.0 / hz));
        lastTickNs = System.nanoTime();
        loopFuture = scheduler.scheduleAtFixedRate(this::tick, 0, periodMs, TimeUnit.MILLISECONDS);
    }

    public synchronized void stop() {
        if (loopFuture != null) {
            loopFuture.cancel(true);
            loopFuture = null;
        }
        scheduler.shutdownNow();
    }

    public void updateScreenFrame(int[] rgbaPixels, int width, int height) {
        this.latestScreenRgba = rgbaPixels;
        this.latestScreenWidth = width;
        this.latestScreenHeight = height;
    }

    public int[] getLastQuantizedRgb() {
        return lastQuantizedRgb;
    }

    public void setComponentState(String comp, boolean state) {
        if ("ALL".equals(comp)) {
            components.put("ALL", state);
            wledDriver.setEnabled(state && Boolean.TRUE.equals(components.get("LEDDEVICE")));
        } else if ("GRABBER".equals(comp) || "SYSTEMGRABBER".equals(comp) || "V4L".equals(comp) || "VIDEOGRABBER".equals(comp)) {
            if ("V4L".equals(comp) || "VIDEOGRABBER".equals(comp)) {
                components.put("VIDEOGRABBER", state);
                components.put("V4L", state);
            } else if ("SYSTEMGRABBER".equals(comp)) {
                components.put("SYSTEMGRABBER", state);
            } else {
                components.put("GRABBER", state);
                components.put("SYSTEMGRABBER", state);
                components.put("VIDEOGRABBER", state);
            }
            boolean anyGrab = Boolean.TRUE.equals(components.get("SYSTEMGRABBER")) || Boolean.TRUE.equals(components.get("VIDEOGRABBER"));
            components.put("GRABBER", anyGrab);
            PriorityEntry vidGrab = priorities.get(240);
            if (vidGrab != null) {
                vidGrab.active = Boolean.TRUE.equals(components.get("VIDEOGRABBER"));
            }
            PriorityEntry sysGrab = priorities.get(250);
            if (sysGrab != null) {
                sysGrab.active = Boolean.TRUE.equals(components.get("SYSTEMGRABBER"));
            }
            recomputeVisiblePriority();
        } else {
            components.put(comp, state);
            if ("LEDDEVICE".equals(comp)) {
                wledDriver.setEnabled(state && Boolean.TRUE.equals(components.get("ALL")));
            }
        }
    }

    public boolean getComponentState(String comp) {
        Boolean v = components.get(comp);
        return v != null ? v : false;
    }

    public JSONArray getComponentsJsonArray() {
        JSONArray arr = new JSONArray();
        String[] order = new String[]{
                "ALL", "SMOOTHING", "BLACKBORDER", "FORWARDER", "BOBLIGHTSERVER",
                "GRABBER", "SYSTEMGRABBER", "VIDEOGRABBER", "FLATBUFSERVER",
                "PROTOSERVER", "RAWUDPSERVER", "LEDDEVICE", "HDR"
        };
        for (String name : order) {
            try {
                JSONObject o = new JSONObject();
                o.put("name", name);
                o.put("enabled", getComponentState(name));
                arr.put(o);
            } catch (Exception ignored) {
            }
        }
        return arr;
    }

    public void setColorPriority(int priority, int[] rgb, long durationMs, String origin) {
        PriorityEntry e = new PriorityEntry();
        e.priority = priority;
        e.componentId = "COLOR";
        e.origin = origin != null ? origin : "Web Configuration";
        e.owner = "Solid Color";
        e.active = true;
        e.durationMs = durationMs;
        e.expiresAtMs = durationMs > 0 ? (System.currentTimeMillis() + durationMs) : -1;
        e.solidRgb = new int[]{
                rgb != null && rgb.length > 0 ? rgb[0] : 255,
                rgb != null && rgb.length > 1 ? rgb[1] : 0,
                rgb != null && rgb.length > 2 ? rgb[2] : 0
        };
        priorities.put(priority, e);
        recomputeVisiblePriority();
    }

    public void setEffectPriority(int priority, String effectName, long durationMs, String origin) {
        PriorityEntry e = new PriorityEntry();
        e.priority = priority;
        e.componentId = "EFFECT";
        e.origin = origin != null ? origin : "Web Configuration";
        e.owner = effectName != null ? effectName : "Rainbow swirl";
        e.effectName = e.owner;
        e.active = true;
        e.durationMs = durationMs;
        e.expiresAtMs = durationMs > 0 ? (System.currentTimeMillis() + durationMs) : -1;
        priorities.put(priority, e);
        recomputeVisiblePriority();
    }

    public void setImagePriority(int priority, int[] rgbPixels, int w, int h, long durationMs, String origin, String name) {
        PriorityEntry e = new PriorityEntry();
        e.priority = priority;
        e.componentId = "IMAGE";
        e.origin = origin != null ? origin : "Web Configuration";
        e.owner = name != null && !name.isEmpty() ? name : "Image";
        e.active = true;
        e.durationMs = durationMs;
        e.expiresAtMs = durationMs > 0 ? (System.currentTimeMillis() + durationMs) : -1;
        e.imageRgb = rgbPixels;
        e.imageWidth = w;
        e.imageHeight = h;
        priorities.put(priority, e);
        recomputeVisiblePriority();
    }

    public void clearPriority(int priority) {
        if (priority < 0) {
            clearAllPriorities();
            return;
        }
        if (priority != 240 && priority != 250) {
            priorities.remove(priority);
        }
        recomputeVisiblePriority();
    }

    public void clearAllPriorities() {
        for (Integer k : new ArrayList<>(priorities.keySet())) {
            if (k < 240) {
                priorities.remove(k);
            }
        }
        recomputeVisiblePriority();
    }

    public void setSourceSelect(boolean auto, int manualPrio) {
        this.prioritiesAutoselect = auto;
        if (!auto) {
            this.manualSelectedPriority = manualPrio;
        }
        recomputeVisiblePriority();
    }

    public boolean isPrioritiesAutoselect() {
        return prioritiesAutoselect;
    }

    public synchronized void recomputeVisiblePriority() {
        long now = System.currentTimeMillis();
        for (Map.Entry<Integer, PriorityEntry> entry : new ArrayList<>(priorities.entrySet())) {
            PriorityEntry p = entry.getValue();
            if (p.expiresAtMs > 0 && now >= p.expiresAtMs) {
                priorities.remove(entry.getKey());
            }
        }
        List<PriorityEntry> list = new ArrayList<>(priorities.values());
        Collections.sort(list, Comparator.comparingInt(a -> a.priority));
        boolean assigned = false;
        for (PriorityEntry p : list) {
            if (!p.active) {
                p.visible = false;
                continue;
            }
            if (!prioritiesAutoselect && p.priority == manualSelectedPriority) {
                p.visible = true;
                assigned = true;
            } else {
                p.visible = false;
            }
        }
        if (!assigned) {
            for (PriorityEntry p : list) {
                if (p.active) {
                    p.visible = true;
                    break;
                }
            }
        }
    }

    public JSONArray getPrioritiesJsonArray() {
        recomputeVisiblePriority();
        List<PriorityEntry> list = new ArrayList<>(priorities.values());
        Collections.sort(list, Comparator.comparingInt(a -> a.priority));
        JSONArray arr = new JSONArray();
        long now = System.currentTimeMillis();
        for (PriorityEntry p : list) {
            try {
                JSONObject o = new JSONObject();
                o.put("priority", p.priority);
                o.put("componentId", p.componentId);
                o.put("origin", p.origin);
                o.put("owner", p.owner);
                o.put("active", p.active);
                o.put("visible", p.visible);
                long rem = p.expiresAtMs > 0 ? Math.max(0L, p.expiresAtMs - now) : -1L;
                o.put("duration_ms", rem);
                if (p.solidRgb != null && p.solidRgb.length == 3) {
                    JSONObject val = new JSONObject();
                    JSONArray rgbArr = new JSONArray();
                    rgbArr.put(p.solidRgb[0]);
                    rgbArr.put(p.solidRgb[1]);
                    rgbArr.put(p.solidRgb[2]);
                    val.put("RGB", rgbArr);
                    o.put("value", val);
                }
                arr.put(o);
            } catch (Exception ignored) {
            }
        }
        return arr;
    }

    private void tick() {
        try {
            long nowNs = System.nanoTime();
            double dtSec = Math.max(0.001, Math.min(0.25, (nowNs - lastTickNs) / 1_000_000_000.0));
            lastTickNs = nowNs;
            effectPhase += dtSec;

            List<LedZone> zones = this.ledZones;
            int ledCount = zones.size();
            if (ledCount == 0) return;
            ensureBufferSize(ledCount);

            if (!getComponentState("ALL")) {
                int[] off = new int[ledCount * 3];
                if (outputListener != null) outputListener.onLedFrame(off);
                return;
            }

            recomputeVisiblePriority();
            PriorityEntry activePrio = null;
            for (PriorityEntry p : priorities.values()) {
                if (p.visible) {
                    activePrio = p;
                    break;
                }
            }

            // Compute raw target RGB (0..255) for each LED before calibration
            float[] rawLedRgb = new float[ledCount * 3];
            if (activePrio != null && "COLOR".equals(activePrio.componentId) && activePrio.solidRgb != null) {
                for (int i = 0; i < ledCount; i++) {
                    rawLedRgb[i * 3] = activePrio.solidRgb[0];
                    rawLedRgb[i * 3 + 1] = activePrio.solidRgb[1];
                    rawLedRgb[i * 3 + 2] = activePrio.solidRgb[2];
                }
            } else if (activePrio != null && "IMAGE".equals(activePrio.componentId) && activePrio.imageRgb != null && activePrio.imageWidth > 0 && activePrio.imageHeight > 0) {
                sampleFrameToLeds(activePrio.imageRgb, activePrio.imageWidth, activePrio.imageHeight, zones, rawLedRgb);
            } else if (activePrio != null && "EFFECT".equals(activePrio.componentId)) {
                renderEffect(activePrio.effectName, ledCount, zones, rawLedRgb);
            } else {
                int[] frame = this.latestScreenRgba;
                int fw = this.latestScreenWidth;
                int fh = this.latestScreenHeight;
                boolean grabEnabled = getComponentState("GRABBER") && (getComponentState("SYSTEMGRABBER") || getComponentState("VIDEOGRABBER"));
                if (frame != null && fw > 0 && fh > 0 && grabEnabled) {
                    sampleFrameToLeds(frame, fw, fh, zones, rawLedRgb);
                } else {
                    renderEffect("Warm mood blobs", ledCount, zones, rawLedRgb);
                }
            }

            // Apply Infinite Color Engine calibration & linear sRGB transformations
            for (int i = 0; i < ledCount; i++) {
                if (zones.get(i).disabled) {
                    targetFloatRgb[i * 3] = 0f;
                    targetFloatRgb[i * 3 + 1] = 0f;
                    targetFloatRgb[i * 3 + 2] = 0f;
                    continue;
                }
                float r = rawLedRgb[i * 3] / 255.0f;
                float g = rawLedRgb[i * 3 + 1] / 255.0f;
                float b = rawLedRgb[i * 3 + 2] / 255.0f;

                // Gamma in linear space
                r = (float) Math.pow(Math.max(0f, Math.min(1f, r)), gammaR);
                g = (float) Math.pow(Math.max(0f, Math.min(1f, g)), gammaG);
                b = (float) Math.pow(Math.max(0f, Math.min(1f, b)), gammaB);

                // 7-Channel RGBCMYW color calibration matrix
                float minRgb = Math.min(r, Math.min(g, b));
                float rPrime = r - minRgb;
                float gPrime = g - minRgb;
                float bPrime = b - minRgb;

                float cyan = Math.min(gPrime, bPrime);
                float magenta = Math.min(rPrime, bPrime);
                float yellow = Math.min(rPrime, gPrime);
                float redPure = Math.max(0f, rPrime - magenta - yellow);
                float greenPure = Math.max(0f, gPrime - cyan - yellow);
                float bluePure = Math.max(0f, bPrime - cyan - magenta);

                float outR = redPure * adjRed[0] + greenPure * adjGreen[0] + bluePure * adjBlue[0]
                        + cyan * adjCyan[0] + magenta * adjMagenta[0] + yellow * adjYellow[0] + minRgb * adjWhite[0];
                float outG = redPure * adjRed[1] + greenPure * adjGreen[1] + bluePure * adjBlue[1]
                        + cyan * adjCyan[1] + magenta * adjMagenta[1] + yellow * adjYellow[1] + minRgb * adjWhite[1];
                float outB = redPure * adjRed[2] + greenPure * adjGreen[2] + bluePure * adjBlue[2]
                        + cyan * adjCyan[2] + magenta * adjMagenta[2] + yellow * adjYellow[2] + minRgb * adjWhite[2];

                // Saturation & luminance gain
                float luma = 0.2126f * outR + 0.7152f * outG + 0.0722f * outB;
                outR = (float) ((luma + (outR - luma) * saturationGain) * luminanceGain);
                outG = (float) ((luma + (outG - luma) * saturationGain) * luminanceGain);
                outB = (float) ((luma + (outB - luma) * saturationGain) * luminanceGain);

                // Overall brightness (0..100%)
                float briScale = Math.max(0f, Math.min(100f, brightness)) / 100.0f;
                outR *= briScale;
                outG *= briScale;
                outB *= briScale;

                // Minimum backlight threshold preservation (prevents signal drop on black scenes/blackbars)
                if (backlightThreshold > 0.0001) {
                    double norm = backlightThreshold > 1.0 ? (backlightThreshold / 100.0) : backlightThreshold;
                    float minFloor = (float) Math.max(1.0f, norm * 255.0f * briScale);
                    if (backlightColored) {
                        outR = Math.max(outR, minFloor);
                        outG = Math.max(outG, minFloor);
                        outB = Math.max(outB, minFloor);
                    } else {
                        float avg = (outR + outG + outB) / 3.0f;
                        if (avg < minFloor) {
                            outR = Math.max(outR, minFloor);
                            outG = Math.max(outG, minFloor);
                            outB = Math.max(outB, minFloor);
                        }
                    }
                }

                targetFloatRgb[i * 3] = Math.max(0f, Math.min(255f, outR));
                targetFloatRgb[i * 3 + 1] = Math.max(0f, Math.min(255f, outG));
                targetFloatRgb[i * 3 + 2] = Math.max(0f, Math.min(255f, outB));
            }

            // Apply HyperHDR v24 smoothing interpolator
            int[] finalRgb = new int[ledCount * 3];
            boolean smoothEnabled = getComponentState("SMOOTHING") && smoothingTimeMs > 0;
            float alpha = (float) Math.min(1.0, (dtSec * 1000.0) / Math.max(15.0, smoothingTimeMs));

            for (int c = 0; c < ledCount * 3; c++) {
                float cur = currentFloatRgb[c];
                float tgt = targetFloatRgb[c];
                float next;
                if (!smoothEnabled) {
                    next = tgt;
                } else if ("hybrid_inertia".equalsIgnoreCase(smoothingType) || "InertialInterpolator".equalsIgnoreCase(smoothingType)) {
                    float stiffness = 18.0f / Math.max(0.05f, smoothingTimeMs / 100.0f);
                    float dampFactor = (float) Math.max(0.15, Math.min(0.98, damping > 1.0 ? damping / 100.0 : damping));
                    float force = (tgt - cur) * stiffness;
                    velocityFloatRgb[c] = (velocityFloatRgb[c] + force * (float) dtSec) * (1.0f - dampFactor * 0.25f);
                    next = cur + velocityFloatRgb[c] * (float) dtSec * 12.0f + (tgt - cur) * alpha * 0.5f;
                } else if ("exponential".equalsIgnoreCase(smoothingType) || "ExponentialInterpolator".equalsIgnoreCase(smoothingType)) {
                    float expAlpha = 1.0f - (float) Math.exp(-dtSec * 1000.0 / Math.max(15.0, smoothingTimeMs * 0.5));
                    next = cur + (tgt - cur) * expAlpha;
                } else {
                    // linear / yuv / HybridRgbInterpolator
                    next = cur + (tgt - cur) * alpha;
                }
                next = Math.max(0f, Math.min(255f, next));
                currentFloatRgb[c] = next;

                // v24 Low-light anti-flickering hysteresis filter
                int q = Math.round(next);
                if (lowLightAntiFlickering && q < 32) {
                    int prevQ = lastQuantizedRgb[c];
                    if (Math.abs(next - prevQ) < 0.72f) {
                        q = prevQ;
                    }
                }
                q = q < 0 ? 0 : (q > 255 ? 255 : q);
                lastQuantizedRgb[c] = q;
                finalRgb[c] = q;
            }

            // Send to WLED hardware & WebSocket live LED stream listeners
            wledDriver.sendLeds(finalRgb);
            if (outputListener != null) {
                outputListener.onLedFrame(finalRgb);
            }

            smoothingFramesWindow++;
            long nowMs = System.currentTimeMillis();
            if (nowMs - smoothingWindowStart >= 1000) {
                smoothingFps = (smoothingFramesWindow * 1000.0) / (nowMs - smoothingWindowStart);
                smoothingFramesWindow = 0;
                smoothingWindowStart = nowMs;
            }
        } catch (Exception ignored) {
        }
    }

    private void sampleFrameToLeds(int[] rgba, int width, int height, List<LedZone> zones, float[] outRgb) {
        if (getComponentState("BLACKBORDER") && blackborderEnabled) {
            detectBlackBorders(rgba, width, height);
        } else {
            detectedCropTop = 0;
            detectedCropBottom = 0;
            detectedCropLeft = 0;
            detectedCropRight = 0;
        }

        int cTop = detectedCropTop;
        int cBottom = Math.max(cTop + 16, height - detectedCropBottom);
        int cLeft = detectedCropLeft;
        int cRight = Math.max(cLeft + 16, width - detectedCropRight);

        int activeW = Math.max(1, cRight - cLeft);
        int activeH = Math.max(1, cBottom - cTop);

        if ("unicolor_mean".equalsIgnoreCase(mappingType)) {
            long sumR = 0, sumG = 0, sumB = 0, count = 0;
            for (int y = cTop; y < cBottom; y += 2) {
                int rowOff = y * width;
                for (int x = cLeft; x < cRight; x += 2) {
                    int px = rgba[rowOff + x];
                    sumR += (px >> 16) & 0xFF;
                    sumG += (px >> 8) & 0xFF;
                    sumB += px & 0xFF;
                    count++;
                }
            }
            float r = count > 0 ? (float) sumR / count : 0f;
            float g = count > 0 ? (float) sumG / count : 0f;
            float b = count > 0 ? (float) sumB / count : 0f;
            for (int i = 0; i < zones.size(); i++) {
                outRgb[i * 3] = r;
                outRgb[i * 3 + 1] = g;
                outRgb[i * 3 + 2] = b;
            }
            return;
        }

        for (int i = 0; i < zones.size(); i++) {
            LedZone z = zones.get(i);
            int x0 = cLeft + Math.max(0, Math.min(activeW - 1, (int) (z.hmin * activeW)));
            int x1 = cLeft + Math.max(x0 - cLeft + 1, Math.min(activeW, (int) Math.ceil(z.hmax * activeW)));
            int y0 = cTop + Math.max(0, Math.min(activeH - 1, (int) (z.vmin * activeH)));
            int y1 = cTop + Math.max(y0 - cTop + 1, Math.min(activeH, (int) Math.ceil(z.vmax * activeH)));
            x1 = Math.min(width, Math.max(x0 + 1, x1));
            y1 = Math.min(height, Math.max(y0 + 1, y1));

            long sumR = 0, sumG = 0, sumB = 0, count = 0;
            for (int y = y0; y < y1; y++) {
                int rowOff = y * width;
                for (int x = x0; x < x1; x++) {
                    int px = rgba[rowOff + x];
                    sumR += (px >> 16) & 0xFF;
                    sumG += (px >> 8) & 0xFF;
                    sumB += px & 0xFF;
                    count++;
                }
            }
            if (count > 0) {
                outRgb[i * 3] = (float) sumR / count;
                outRgb[i * 3 + 1] = (float) sumG / count;
                outRgb[i * 3 + 2] = (float) sumB / count;
            }
        }
    }

    private void detectBlackBorders(int[] rgba, int width, int height) {
        int thresh = Math.max(1, (blackborderThreshold * 255) / 100);
        int maxBorderH = height / 3;
        int maxBorderW = width / 3;

        int xMid = width / 2;
        int x33 = width / 3;
        int x66 = (width * 2) / 3;
        int x25 = width / 4;
        int x75 = (width * 3) / 4;

        int yMid = height / 2;
        int y33 = height / 3;
        int y66 = (height * 2) / 3;

        // 1. Horizontal borders (Letterbox: top & bottom black bars)
        int detectedY = -1;
        for (int y = 0; y < maxBorderH; y++) {
            int topRow = y * width;
            int botRow = (height - 1 - y) * width;
            if (isPixelBright(rgba[topRow + xMid], thresh)
                    || isPixelBright(rgba[topRow + x33], thresh)
                    || isPixelBright(rgba[topRow + x66], thresh)
                    || isPixelBright(rgba[topRow + x25], thresh)
                    || isPixelBright(rgba[topRow + x75], thresh)
                    || isPixelBright(rgba[botRow + xMid], thresh)
                    || isPixelBright(rgba[botRow + x33], thresh)
                    || isPixelBright(rgba[botRow + x66], thresh)) {
                detectedY = y;
                break;
            }
        }

        if (detectedY == -1) {
            // Dark scene / unknown border: PRESERVE current detected border!
            unknownFramesY++;
            if (unknownFramesY >= blackborderUnknownTimeout) {
                detectedCropTop = 0;
                detectedCropBottom = 0;
                candidateCropY = 0;
                consistentFramesY = 0;
            }
        } else {
            unknownFramesY = 0;
            int yBorder = detectedY > 0 ? Math.min(maxBorderH, detectedY + blackborderBlurRemove) : 0;
            if (Math.abs(yBorder - candidateCropY) <= 2) {
                consistentFramesY++;
                if (consistentFramesY >= blackborderBorderSwitchCnt && Math.abs(yBorder - detectedCropTop) > 1) {
                    detectedCropTop = yBorder;
                    detectedCropBottom = yBorder;
                }
            } else {
                candidateCropY = yBorder;
                consistentFramesY = 0;
            }
        }

        // 2. Vertical borders (Pillarbox: left & right black bars)
        int detectedX = -1;
        for (int x = 0; x < maxBorderW; x++) {
            int rightX = width - 1 - x;
            if (isPixelBright(rgba[yMid * width + x], thresh)
                    || isPixelBright(rgba[y33 * width + x], thresh)
                    || isPixelBright(rgba[y66 * width + x], thresh)
                    || isPixelBright(rgba[yMid * width + rightX], thresh)
                    || isPixelBright(rgba[y33 * width + rightX], thresh)
                    || isPixelBright(rgba[y66 * width + rightX], thresh)) {
                detectedX = x;
                break;
            }
        }

        if (detectedX == -1) {
            // Dark scene / unknown border: PRESERVE current detected border!
            unknownFramesX++;
            if (unknownFramesX >= blackborderUnknownTimeout) {
                detectedCropLeft = 0;
                detectedCropRight = 0;
                candidateCropX = 0;
                consistentFramesX = 0;
            }
        } else {
            unknownFramesX = 0;
            int xBorder = detectedX > 0 ? Math.min(maxBorderW, detectedX + blackborderBlurRemove) : 0;
            if (Math.abs(xBorder - candidateCropX) <= 2) {
                consistentFramesX++;
                if (consistentFramesX >= blackborderBorderSwitchCnt && Math.abs(xBorder - detectedCropLeft) > 1) {
                    detectedCropLeft = xBorder;
                    detectedCropRight = xBorder;
                }
            } else {
                candidateCropX = xBorder;
                consistentFramesX = 0;
            }
        }
    }

    private static boolean isPixelBright(int argb, int threshold) {
        int r = (argb >> 16) & 0xFF;
        int g = (argb >> 8) & 0xFF;
        int b = argb & 0xFF;
        return r > threshold || g > threshold || b > threshold;
    }

    private void renderEffect(String effectName, int ledCount, List<LedZone> zones, float[] outRgb) {
        String name = effectName != null ? effectName.toLowerCase() : "rainbow swirl";
        double speed = name.contains("fast") ? 1.8 : 0.6;
        for (int i = 0; i < ledCount; i++) {
            double pos = (double) i / Math.max(1, ledCount);
            float h, s = 1.0f, v = 1.0f;
            if (name.contains("police")) {
                boolean half = ((int) (effectPhase * 6.0) % 2) == 0;
                if (i < ledCount / 2) {
                    outRgb[i * 3] = half ? 255 : 0;
                    outRgb[i * 3 + 1] = 0;
                    outRgb[i * 3 + 2] = half ? 0 : 255;
                } else {
                    outRgb[i * 3] = half ? 0 : 255;
                    outRgb[i * 3 + 1] = 0;
                    outRgb[i * 3 + 2] = half ? 255 : 0;
                }
                continue;
            } else if (name.contains("knight")) {
                double scan = 0.5 + 0.5 * Math.sin(effectPhase * 3.2);
                double dist = Math.abs(pos - scan);
                float intensity = (float) Math.max(0.0, 1.0 - dist * 6.0);
                outRgb[i * 3] = 255f * intensity;
                outRgb[i * 3 + 1] = 0f;
                outRgb[i * 3 + 2] = 0f;
                continue;
            } else if (name.contains("candle") || name.contains("warm")) {
                double wave = 0.75 + 0.25 * Math.sin(effectPhase * 2.3 + i * 0.35);
                outRgb[i * 3] = (float) (255 * wave);
                outRgb[i * 3 + 1] = (float) (130 * wave);
                outRgb[i * 3 + 2] = (float) (25 * wave);
                continue;
            } else if (name.contains("sea")) {
                h = (float) (0.50 + 0.12 * Math.sin(effectPhase * 1.2 + pos * Math.PI * 2));
            } else if (name.contains("plasma") || name.contains("atomic")) {
                h = (float) ((0.5 + 0.5 * Math.sin(effectPhase * 1.5 + pos * Math.PI * 4)) % 1.0);
            } else {
                h = (float) ((pos + effectPhase * speed * 0.25) % 1.0);
                if (h < 0) h += 1.0f;
            }
            int[] rgb = hsvToRgb(h, s, v);
            outRgb[i * 3] = rgb[0];
            outRgb[i * 3 + 1] = rgb[1];
            outRgb[i * 3 + 2] = rgb[2];
        }
    }

    private static int[] hsvToRgb(float h, float s, float v) {
        int i = (int) (h * 6);
        float f = h * 6 - i;
        float p = v * (1 - s);
        float q = v * (1 - f * s);
        float t = v * (1 - (1 - f) * s);
        float r, g, b;
        switch (i % 6) {
            case 0: r = v; g = t; b = p; break;
            case 1: r = q; g = v; b = p; break;
            case 2: r = p; g = v; b = t; break;
            case 3: r = p; g = q; b = v; break;
            case 4: r = t; g = p; b = v; break;
            default: r = v; g = p; b = q; break;
        }
        return new int[]{Math.round(r * 255), Math.round(g * 255), Math.round(b * 255)};
    }

    public String getMappingType() {
        return mappingType;
    }

    public void setMappingType(String mappingType) {
        this.mappingType = mappingType;
    }

    public String getVideoMode() {
        return videoMode;
    }

    public void setVideoMode(String videoMode) {
        this.videoMode = videoMode;
    }

    public int getVideoModeHdr() {
        return videoModeHdr;
    }

    public void setVideoModeHdr(int videoModeHdr) {
        this.videoModeHdr = videoModeHdr;
    }

    public double getSmoothingFps() {
        return smoothingFps;
    }

    public int getLedCount() {
        return ledZones.size();
    }

    private static int[] optRgbArray(JSONArray arr, int[] def) {
        if (arr == null || arr.length() < 3) return def;
        return new int[]{arr.optInt(0, def[0]), arr.optInt(1, def[1]), arr.optInt(2, def[2])};
    }

    private static JSONArray toJsonArray(int[] rgb) {
        JSONArray a = new JSONArray();
        for (int v : rgb) a.put(v);
        return a;
    }
}
