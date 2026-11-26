package org.hyperhdr.androidtv;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.HttpURLConnection;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.NetworkInterface;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Enumeration;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;

public class WledDriver {
    private volatile String deviceType = "wled";
    private volatile String host = "192.168.1.150";
    private volatile int port = 21324;
    private volatile String colorOrder = "rgb";
    private volatile boolean invert = false;
    private volatile boolean brightnessMax = true;
    private volatile int brightnessMaxLevel = 255;
    private volatile boolean restoreOriginalState = false;
    private volatile boolean enabled = true;

    private DatagramSocket udpSocket;
    private InetAddress resolvedAddress;
    private String lastResolvedHost = "";
    private long lastDnsAttemptMs = 0;
    private String savedWledStateJson = null;

    // Performance metrics for HyperHDR dashboard
    private volatile long packetsSentWindow = 0;
    private volatile long packetsDroppedWindow = 0;
    private volatile long totalPacketsSent = 0;
    private volatile double currentLedFps = 0.0;
    private volatile long lastWindowTimestamp = System.currentTimeMillis();

    public WledDriver() {
        try {
            udpSocket = new DatagramSocket();
        } catch (Exception ignored) {
        }
    }

    public synchronized void configure(JSONObject deviceCfg) {
        if (deviceCfg == null) return;
        String newType = deviceCfg.optString("type", "wled").toLowerCase();
        String newHost = deviceCfg.optString("host", deviceCfg.optString("output", host)).trim();
        if (newHost.contains(":")) {
            String[] parts = newHost.split(":", 2);
            newHost = parts[0];
            try {
                this.port = Integer.parseInt(parts[1]);
            } catch (NumberFormatException ignored) {
            }
        } else {
            if ("ddp".equals(newType) || "hyperk".equals(newType)) {
                this.port = deviceCfg.optInt("port", 4048);
            } else {
                this.port = deviceCfg.optInt("port", 21324);
            }
        }
        this.deviceType = newType;
        if (!newHost.isEmpty()) {
            this.host = newHost;
        }
        this.colorOrder = deviceCfg.optString("colorOrder", "rgb").toLowerCase();
        this.invert = deviceCfg.optBoolean("invert", false);
        this.brightnessMax = deviceCfg.optBoolean("brightnessMax", true);
        this.brightnessMaxLevel = deviceCfg.optInt("brightnessMaxLevel", 255);
        this.restoreOriginalState = deviceCfg.optBoolean("restoreOriginalState", false);
        this.lastResolvedHost = "";
        this.lastDnsAttemptMs = 0;
        final String preHost = this.host;
        new Thread(() -> {
            try {
                InetAddress addr = InetAddress.getByName(preHost);
                synchronized (WledDriver.this) {
                    if (preHost.equals(WledDriver.this.host)) {
                        resolvedAddress = addr;
                        lastResolvedHost = preHost;
                    }
                }
            } catch (Exception ignored) {
            }
        }).start();

        if (enabled && ("wled".equals(deviceType) || "hyperk".equals(deviceType))) {
            final String targetHost = this.host;
            final boolean setBri = this.brightnessMax;
            final int briLevel = this.brightnessMaxLevel;
            final boolean saveState = this.restoreOriginalState;
            new Thread(() -> prepareWledStreamState(targetHost, setBri, briLevel, saveState)).start();
        }
    }

    public void setEnabled(boolean en) {
        boolean wasEnabled = this.enabled;
        this.enabled = en;
        if (!en && wasEnabled && restoreOriginalState && savedWledStateJson != null) {
            final String targetHost = this.host;
            final String restorePayload = this.savedWledStateJson;
            new Thread(() -> httpPostJson("http://" + targetHost + "/json/state", restorePayload, 800)).start();
        } else if (en && !wasEnabled && ("wled".equals(deviceType) || "hyperk".equals(deviceType))) {
            final String targetHost = this.host;
            new Thread(() -> prepareWledStreamState(targetHost, brightnessMax, brightnessMaxLevel, restoreOriginalState)).start();
        }
    }

    public boolean isEnabled() {
        return enabled;
    }

    public String getHost() {
        return host;
    }

    public String getDeviceType() {
        return deviceType;
    }

    public double getCurrentLedFps() {
        return currentLedFps;
    }

    public long getTotalPacketsSent() {
        return totalPacketsSent;
    }

    public long getPacketsDroppedWindow() {
        return packetsDroppedWindow;
    }

    private void prepareWledStreamState(String targetHost, boolean setBri, int briLevel, boolean saveState) {
        if (targetHost == null || targetHost.isEmpty()) return;
        try {
            if (saveState) {
                String stateResp = httpGet("http://" + targetHost + "/json/state", 700);
                if (stateResp != null && stateResp.startsWith("{")) {
                    savedWledStateJson = stateResp;
                }
            }
            JSONObject body = new JSONObject();
            body.put("on", true);
            if (setBri) {
                body.put("bri", Math.max(1, Math.min(255, briLevel)));
            }
            httpPostJson("http://" + targetHost + "/json/state", body.toString(), 700);
        } catch (Exception ignored) {
        }
    }

    public void sendLeds(int[] rgbFlat) {
        if (!enabled || rgbFlat == null || rgbFlat.length < 3 || host == null || host.isEmpty()) {
            return;
        }
        if ("file".equals(deviceType)) {
            recordFrameSent(true);
            return;
        }
        try {
            if (udpSocket == null || udpSocket.isClosed()) {
                udpSocket = new DatagramSocket();
            }
            if (resolvedAddress == null || !host.equals(lastResolvedHost)) {
                long now = System.currentTimeMillis();
                if (now - lastDnsAttemptMs < 10000L) {
                    recordFrameSent(false);
                    return;
                }
                lastDnsAttemptMs = now;
                try {
                    resolvedAddress = InetAddress.getByName(host);
                    lastResolvedHost = host;
                } catch (Exception e) {
                    lastResolvedHost = host;
                    resolvedAddress = null;
                    recordFrameSent(false);
                    return;
                }
            }
            if (resolvedAddress == null) {
                recordFrameSent(false);
                return;
            }
            int ledCount = rgbFlat.length / 3;
            byte[] ordered = new byte[ledCount * 3];
            for (int i = 0; i < ledCount; i++) {
                int r = clamp8(rgbFlat[i * 3]);
                int g = clamp8(rgbFlat[i * 3 + 1]);
                int b = clamp8(rgbFlat[i * 3 + 2]);
                if (invert) {
                    r = 255 - r;
                    g = 255 - g;
                    b = 255 - b;
                }
                int b0 = r, b1 = g, b2 = b;
                switch (colorOrder) {
                    case "bgr": b0 = b; b1 = g; b2 = r; break;
                    case "grb": b0 = g; b1 = r; b2 = b; break;
                    case "brg": b0 = b; b1 = r; b2 = g; break;
                    case "gbr": b0 = g; b1 = b; b2 = r; break;
                    case "rbg": b0 = r; b1 = b; b2 = g; break;
                    default: break;
                }
                ordered[i * 3] = (byte) b0;
                ordered[i * 3 + 1] = (byte) b1;
                ordered[i * 3 + 2] = (byte) b2;
            }

            if ("ddp".equals(deviceType) || "hyperk".equals(deviceType)) {
                sendDdpPackets(ordered);
            } else if ("udpraw".equals(deviceType)) {
                DatagramPacket pkt = new DatagramPacket(ordered, ordered.length, resolvedAddress, port);
                udpSocket.send(pkt);
            } else {
                // Default WLED UDP Realtime protocol on port 21324:
                // Protocol 2 = DRGB (up to 490 LEDs), Protocol 4 = DNRGB (chunked for > 490 LEDs)
                int targetPort = (port > 0) ? port : 21324;
                if (ledCount <= 490) {
                    byte[] pktBytes = new byte[2 + ordered.length];
                    pktBytes[0] = 2; // WLED_NOTIFIER_DRGB
                    pktBytes[1] = 2; // 2 second timeout before returning to normal WLED mode
                    System.arraycopy(ordered, 0, pktBytes, 2, ordered.length);
                    DatagramPacket pkt = new DatagramPacket(pktBytes, pktBytes.length, resolvedAddress, targetPort);
                    udpSocket.send(pkt);
                } else {
                    int chunkLeds = 489;
                    for (int startLed = 0; startLed < ledCount; startLed += chunkLeds) {
                        int count = Math.min(chunkLeds, ledCount - startLed);
                        byte[] pktBytes = new byte[4 + count * 3];
                        pktBytes[0] = 4; // WLED_NOTIFIER_DNRGB
                        pktBytes[1] = 2; // 2 second timeout
                        pktBytes[2] = (byte) ((startLed >> 8) & 0xFF);
                        pktBytes[3] = (byte) (startLed & 0xFF);
                        System.arraycopy(ordered, startLed * 3, pktBytes, 4, count * 3);
                        DatagramPacket pkt = new DatagramPacket(pktBytes, pktBytes.length, resolvedAddress, targetPort);
                        udpSocket.send(pkt);
                    }
                }
            }
            recordFrameSent(true);
        } catch (Exception e) {
            recordFrameSent(false);
        }
    }

    private void sendDdpPackets(byte[] orderedRgb) throws Exception {
        int targetPort = (port > 0 && port != 21324) ? port : 4048;
        int maxDataBytes = 480 * 3; // 1440 bytes per DDP packet
        int totalBytes = orderedRgb.length;
        int offset = 0;
        int seq = 1;
        while (offset < totalBytes) {
            int len = Math.min(maxDataBytes, totalBytes - offset);
            boolean isLast = (offset + len) >= totalBytes;
            byte[] ddp = new byte[10 + len];
            ddp[0] = (byte) (0x40 | (isLast ? 0x01 : 0x00)); // V1 + Push flag on last packet
            ddp[1] = (byte) (seq & 0x0F);
            ddp[2] = 0x01; // RGB data type
            ddp[3] = 0x01; // Default destination ID 1
            ddp[4] = (byte) ((offset >> 24) & 0xFF);
            ddp[5] = (byte) ((offset >> 16) & 0xFF);
            ddp[6] = (byte) ((offset >> 8) & 0xFF);
            ddp[7] = (byte) (offset & 0xFF);
            ddp[8] = (byte) ((len >> 8) & 0xFF);
            ddp[9] = (byte) (len & 0xFF);
            System.arraycopy(orderedRgb, offset, ddp, 10, len);
            DatagramPacket pkt = new DatagramPacket(ddp, ddp.length, resolvedAddress, targetPort);
            udpSocket.send(pkt);
            offset += len;
            seq++;
        }
    }

    private void recordFrameSent(boolean ok) {
        if (ok) {
            packetsSentWindow++;
            totalPacketsSent++;
        } else {
            packetsDroppedWindow++;
        }
        long now = System.currentTimeMillis();
        long elapsed = now - lastWindowTimestamp;
        if (elapsed >= 1000) {
            currentLedFps = (packetsSentWindow * 1000.0) / elapsed;
            packetsSentWindow = 0;
            lastWindowTimestamp = now;
        }
    }

    public JSONArray discoverDevices() {
        JSONArray devices = new JSONArray();
        try {
            String localIp = getLocalIpv4Address();
            List<String> candidates = new ArrayList<>();
            if (host != null && !host.isEmpty() && !host.equals("127.0.0.1")) {
                candidates.add(host);
            }
            if (localIp != null && localIp.contains(".")) {
                String prefix = localIp.substring(0, localIp.lastIndexOf('.') + 1);
                for (int i = 1; i <= 254; i++) {
                    String ip = prefix + i;
                    if (!candidates.contains(ip)) {
                        candidates.add(ip);
                    }
                }
            }
            ExecutorService pool = Executors.newFixedThreadPool(32);
            List<Callable<JSONObject>> tasks = new ArrayList<>();
            for (final String candidateIp : candidates) {
                tasks.add(() -> probeWledHost(candidateIp));
            }
            List<Future<JSONObject>> results = pool.invokeAll(tasks, 1800, TimeUnit.MILLISECONDS);
            pool.shutdownNow();
            for (Future<JSONObject> fut : results) {
                if (fut.isDone() && !fut.isCancelled()) {
                    try {
                        JSONObject item = fut.get();
                        if (item != null) {
                            devices.put(item);
                        }
                    } catch (Exception ignored) {
                    }
                }
            }
        } catch (Exception ignored) {
        }
        if (devices.length() == 0 && host != null && !host.isEmpty()) {
            try {
                JSONObject fallback = new JSONObject();
                fallback.put("name", host + " (Configured WLED)");
                fallback.put("value", host);
                fallback.put("ip", host);
                fallback.put("port", 80);
                devices.put(fallback);
            } catch (Exception ignored) {
            }
        }
        return devices;
    }

    private JSONObject probeWledHost(String ip) {
        try {
            String jsonStr = httpGet("http://" + ip + "/json/info", 220);
            if (jsonStr != null && jsonStr.contains("\"leds\"")) {
                JSONObject info = new JSONObject(jsonStr);
                String name = info.optString("name", "WLED");
                String ver = info.optString("ver", "");
                int count = 0;
                JSONObject ledsObj = info.optJSONObject("leds");
                if (ledsObj != null) {
                    count = ledsObj.optInt("count", 0);
                }
                JSONObject dev = new JSONObject();
                dev.put("name", name + " (" + ip + (count > 0 ? ", " + count + " LEDs" : "") + (ver.isEmpty() ? "" : ", v" + ver) + ")");
                dev.put("value", ip);
                dev.put("ip", ip);
                dev.put("host", name);
                dev.put("port", 80);
                dev.put("ledCount", count);
                return dev;
            }
        } catch (Exception ignored) {
        }
        return null;
    }

    public JSONObject queryDeviceProperties(String targetHost) {
        String h = (targetHost != null && !targetHost.isEmpty()) ? targetHost : this.host;
        JSONObject props = new JSONObject();
        try {
            String jsonStr = httpGet("http://" + h + "/json/info", 800);
            if (jsonStr != null && jsonStr.startsWith("{")) {
                JSONObject info = new JSONObject(jsonStr);
                props.put("info", info);
                JSONObject ledsObj = info.optJSONObject("leds");
                if (ledsObj != null) {
                    props.put("ledCount", ledsObj.optInt("count", 80));
                }
            }
        } catch (Exception ignored) {
        }
        return props;
    }

    public void identifyDevice(String targetHost) {
        final String h = (targetHost != null && !targetHost.isEmpty()) ? targetHost : this.host;
        new Thread(() -> {
            try {
                InetAddress addr = InetAddress.getByName(h);
                DatagramSocket sock = new DatagramSocket();
                int[][] colors = new int[][]{{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255}};
                for (int c = 0; c < colors.length; c++) {
                    byte[] pkt = new byte[2 + 120 * 3];
                    pkt[0] = 2; // DRGB
                    pkt[1] = 2;
                    for (int i = 0; i < 120; i++) {
                        pkt[2 + i * 3] = (byte) colors[c][0];
                        pkt[2 + i * 3 + 1] = (byte) colors[c][1];
                        pkt[2 + i * 3 + 2] = (byte) colors[c][2];
                    }
                    sock.send(new DatagramPacket(pkt, pkt.length, addr, 21324));
                    Thread.sleep(350);
                }
                sock.close();
            } catch (Exception ignored) {
            }
        }).start();
    }

    public static String getLocalIpv4Address() {
        try {
            Enumeration<NetworkInterface> interfaces = NetworkInterface.getNetworkInterfaces();
            for (NetworkInterface intf : Collections.list(interfaces)) {
                if (!intf.isUp() || intf.isLoopback()) continue;
                for (InetAddress addr : Collections.list(intf.getInetAddresses())) {
                    if (!addr.isLoopbackAddress() && addr instanceof Inet4Address) {
                        return addr.getHostAddress();
                    }
                }
            }
        } catch (Exception ignored) {
        }
        return "127.0.0.1";
    }

    private static String httpGet(String urlStr, int timeoutMs) {
        HttpURLConnection conn = null;
        try {
            URL url = new URL(urlStr);
            conn = (HttpURLConnection) url.openConnection();
            conn.setRequestMethod("GET");
            conn.setConnectTimeout(timeoutMs);
            conn.setReadTimeout(timeoutMs);
            if (conn.getResponseCode() == 200) {
                BufferedReader br = new BufferedReader(new InputStreamReader(conn.getInputStream(), StandardCharsets.UTF_8));
                StringBuilder sb = new StringBuilder();
                String line;
                while ((line = br.readLine()) != null) {
                    sb.append(line);
                }
                br.close();
                return sb.toString();
            }
        } catch (Exception ignored) {
        } finally {
            if (conn != null) conn.disconnect();
        }
        return null;
    }

    private static boolean httpPostJson(String urlStr, String jsonBody, int timeoutMs) {
        HttpURLConnection conn = null;
        try {
            URL url = new URL(urlStr);
            conn = (HttpURLConnection) url.openConnection();
            conn.setRequestMethod("POST");
            conn.setConnectTimeout(timeoutMs);
            conn.setReadTimeout(timeoutMs);
            conn.setRequestProperty("Content-Type", "application/json");
            conn.setDoOutput(true);
            byte[] bytes = jsonBody.getBytes(StandardCharsets.UTF_8);
            try (OutputStream os = conn.getOutputStream()) {
                os.write(bytes);
            }
            return conn.getResponseCode() >= 200 && conn.getResponseCode() < 300;
        } catch (Exception ignored) {
        } finally {
            if (conn != null) conn.disconnect();
        }
        return false;
    }

    private static int clamp8(int v) {
        return v < 0 ? 0 : (v > 255 ? 255 : v);
    }
}
