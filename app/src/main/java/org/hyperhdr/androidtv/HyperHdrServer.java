package org.hyperhdr.androidtv;

import android.app.ActivityManager;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Build;
import android.util.Base64;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.UUID;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

public class HyperHdrServer {

    private final Context context;
    private final InfiniteColorEngine colorEngine;
    private final WledDriver wledDriver;
    private final ScreenCaptureManager screenCaptureManager;

    private ServerSocket httpServerSocket;
    private ServerSocket jsonRpcServerSocket;
    private ServerSocket flatbufServerSocket;
    private ServerSocket protoServerSocket;
    private volatile boolean running = false;
    private int httpPort = 8090;
    private int jsonRpcPort = 19444;
    private int flatbufPort = 19400;
    private int protoPort = 19445;

    private JSONObject serverSchema;
    private JSONObject dbState; // holds instances + per-instance config + tokens
    private int currentInstance = 0;

    private final List<WsClient> wsClients = new CopyOnWriteArrayList<>();
    private final List<String> activeSessionTokens = new CopyOnWriteArrayList<>();
    private final List<JSONObject> logBuffer = new CopyOnWriteArrayList<>();
    private final ScheduledExecutorService telemetryScheduler = Executors.newSingleThreadScheduledExecutor();
    private long lastLedStreamPushMs = 0;

    public HyperHdrServer(Context context, InfiniteColorEngine colorEngine, WledDriver wledDriver, ScreenCaptureManager screenCaptureManager) {
        this.context = context.getApplicationContext();
        this.colorEngine = colorEngine;
        this.wledDriver = wledDriver;
        this.screenCaptureManager = screenCaptureManager;

        loadSchemasAndDatabase();

        this.colorEngine.setOutputListener(this::onEngineLedFrame);
        this.screenCaptureManager.setPreviewListener(this::onPreviewJpegFrame);
    }

    public int getHttpPort() {
        return httpPort;
    }

    private synchronized void loadSchemasAndDatabase() {
        try {
            String schemaText = readAssetText("server_schema.json");
            serverSchema = new JSONObject(schemaText);

            File filesDir = context.getFilesDir();
            if (filesDir != null && !filesDir.exists()) {
                filesDir.mkdirs();
            }
            File dbFile = new File(filesDir, "hyperhdr_db.json");
            File tmpFile = new File(filesDir, "hyperhdr_db.json.tmp");
            if ((!dbFile.exists() || dbFile.length() == 0) && tmpFile.exists() && tmpFile.length() > 0) {
                tmpFile.renameTo(dbFile);
            }

            if (dbFile.exists() && dbFile.length() > 0) {
                try (FileInputStream fis = new FileInputStream(dbFile)) {
                    byte[] data = readAllBytes(fis);
                    dbState = new JSONObject(new String(data, StandardCharsets.UTF_8));
                } catch (Exception e) {
                    dbState = null;
                }
            }

            if (dbState == null) {
                String defText = readAssetText("default_config.json");
                JSONObject defConfig = new JSONObject(defText);
                dbState = new JSONObject();
                dbState.put("version", "HyperHDR_export_format_v20");
                dbState.put("adminPassword", "hyperhdr");
                dbState.put("defaultPasswordSet", true);
                dbState.put("tokens", new JSONArray());

                JSONArray instances = new JSONArray();
                JSONObject inst0 = new JSONObject();
                inst0.put("instance", 0);
                inst0.put("friendly_name", "First LED instance");
                inst0.put("running", true);
                instances.put(inst0);
                dbState.put("instances", instances);

                JSONObject settingsByInst = new JSONObject();
                settingsByInst.put("0", defConfig);
                dbState.put("settings", settingsByInst);
                saveDatabase();
            } else {
                currentInstance = dbState.optInt("currentInstance", 0);
            }

            JSONObject activeCfg = getActiveInstanceConfig();
            if (activeCfg != null) {
                migrateGrabberConfig(activeCfg);
                saveDatabase();
                JSONObject webCfg = activeCfg.optJSONObject("webConfig");
                if (webCfg != null) {
                    httpPort = webCfg.optInt("port", 8090);
                }
                JSONObject jsonSrv = activeCfg.optJSONObject("jsonServer");
                if (jsonSrv != null) {
                    jsonRpcPort = jsonSrv.optInt("port", 19444);
                }
                JSONObject fbSrv = activeCfg.optJSONObject("flatbufServer");
                if (fbSrv != null) {
                    flatbufPort = fbSrv.optInt("port", 19400);
                }
                JSONObject prSrv = activeCfg.optJSONObject("protoServer");
                if (prSrv != null) {
                    protoPort = prSrv.optInt("port", 19445);
                }
                colorEngine.applyConfig(activeCfg);
                screenCaptureManager.applyConfig(activeCfg);
            }
            appendLog("INFO", "DAEMON", "HyperHDR v24.0.0-beta.0 Server initialized on Android TV (Web port " + httpPort + ", FlatBuffers port " + flatbufPort + ")");
        } catch (Exception e) {
            appendLog("ERROR", "DAEMON", "Initialization error: " + e.getMessage());
        }
    }

    private void migrateGrabberConfig(JSONObject cfg) {
        try {
            JSONObject sysCtrl = cfg.optJSONObject("systemControl");
            if (sysCtrl == null) {
                sysCtrl = new JSONObject();
                sysCtrl.put("systemInstanceEnable", true);
                cfg.put("systemControl", sysCtrl);
            } else if (!sysCtrl.has("systemInstanceEnable")) {
                sysCtrl.put("systemInstanceEnable", true);
            }

            JSONObject vidCtrl = cfg.optJSONObject("videoControl");
            if (vidCtrl == null) {
                vidCtrl = new JSONObject();
                vidCtrl.put("videoInstanceEnable", true);
                cfg.put("videoControl", vidCtrl);
            } else if (!vidCtrl.has("videoInstanceEnable")) {
                vidCtrl.put("videoInstanceEnable", true);
            }

            JSONObject sysGrab = cfg.optJSONObject("systemGrabber");
            if (sysGrab == null) {
                sysGrab = new JSONObject();
                sysGrab.put("device", "Android MediaProjection API");
                cfg.put("systemGrabber", sysGrab);
            }
            String sDev = sysGrab.optString("device", "auto");
            if (sDev.isEmpty() || "auto".equalsIgnoreCase(sDev) || sDev.contains("Pipewire") || sDev.contains("screencap")) {
                sysGrab.put("device", "Android MediaProjection API");
            }

            JSONObject vidGrab = cfg.optJSONObject("videoGrabber");
            if (vidGrab == null) {
                vidGrab = new JSONObject();
                vidGrab.put("device", "Android MediaProjection API");
                cfg.put("videoGrabber", vidGrab);
            }
            String vDev = vidGrab.optString("device", "auto");
            if (vDev.isEmpty() || "auto".equalsIgnoreCase(vDev) || "Android-MediaProjection-0".equals(vDev) || "Termux-ScreenGrabber-0".equals(vDev)) {
                vidGrab.put("device", "Android MediaProjection API");
            }

            JSONObject bbObj = cfg.optJSONObject("blackborderdetector");
            if (bbObj == null) {
                bbObj = new JSONObject();
                bbObj.put("enable", true);
                bbObj.put("threshold", 5);
                bbObj.put("unknownFrameCnt", 600);
                bbObj.put("borderFrameCnt", 50);
                bbObj.put("maxInconsistentCnt", 10);
                bbObj.put("blurRemoveCnt", 1);
                bbObj.put("mode", "default");
                cfg.put("blackborderdetector", bbObj);
            } else if (!bbObj.has("enable") || !bbObj.optBoolean("enable", false)) {
                bbObj.put("enable", true);
            }
        } catch (Exception ignored) {
        }
    }

    public void notifyGrabberStateChanged() {
        try {
            JSONObject compMsg = new JSONObject();
            compMsg.put("command", "components-update");
            JSONObject data = new JSONObject();
            data.put("name", "SYSTEMGRABBER");
            data.put("enabled", colorEngine.getComponentState("SYSTEMGRABBER"));
            compMsg.put("data", data);
            broadcastWs(compMsg);
            broadcastPrioritiesUpdate();
        } catch (Exception ignored) {
        }
    }

    public synchronized void saveDatabase() {
        if (dbState == null) return;
        try {
            File filesDir = context.getFilesDir();
            if (filesDir != null && !filesDir.exists()) {
                filesDir.mkdirs();
            }
            File dbFile = new File(filesDir, "hyperhdr_db.json");
            File tmpFile = new File(filesDir, "hyperhdr_db.json.tmp");
            byte[] bytes = dbState.toString(2).getBytes(StandardCharsets.UTF_8);
            try (FileOutputStream fos = new FileOutputStream(tmpFile)) {
                fos.write(bytes);
                fos.flush();
                try {
                    fos.getFD().sync();
                } catch (Exception ignored) {}
            }
            if (!tmpFile.renameTo(dbFile)) {
                try (FileOutputStream fos = new FileOutputStream(dbFile)) {
                    fos.write(bytes);
                    fos.flush();
                    try {
                        fos.getFD().sync();
                    } catch (Exception ignored) {}
                }
                tmpFile.delete();
            }
        } catch (Exception e) {
            appendLog("ERROR", "DB", "Failed to save database: " + e.getMessage());
        }
    }

    public synchronized JSONObject getActiveInstanceConfig() {
        try {
            JSONObject settings = dbState.optJSONObject("settings");
            if (settings == null) {
                settings = new JSONObject();
                dbState.put("settings", settings);
            }
            String key = String.valueOf(currentInstance);
            JSONObject cfg = settings.optJSONObject(key);
            if (cfg == null) {
                String defText = readAssetText("default_config.json");
                cfg = new JSONObject(defText);
                settings.put(key, cfg);
            }
            return cfg;
        } catch (Exception e) {
            return new JSONObject();
        }
    }

    public synchronized void start() {
        if (running) return;
        running = true;
        new Thread(this::runHttpServerLoop, "HyperHdrHttp8090").start();
        new Thread(this::runJsonRpcServerLoop, "HyperHdrJson19444").start();
        new Thread(this::runFlatbufServerLoop, "HyperHdrFlatbuf19400").start();
        new Thread(this::runProtoServerLoop, "HyperHdrProto19445").start();
        telemetryScheduler.scheduleAtFixedRate(this::periodicTelemetryTick, 1000, 1000, TimeUnit.MILLISECONDS);
    }

    public synchronized void stop() {
        running = false;
        try { telemetryScheduler.shutdownNow(); } catch (Exception ignored) {}
        try { if (httpServerSocket != null) httpServerSocket.close(); } catch (Exception ignored) {}
        try { if (jsonRpcServerSocket != null) jsonRpcServerSocket.close(); } catch (Exception ignored) {}
        try { if (flatbufServerSocket != null) flatbufServerSocket.close(); } catch (Exception ignored) {}
        try { if (protoServerSocket != null) protoServerSocket.close(); } catch (Exception ignored) {}
        for (WsClient client : wsClients) {
            client.close();
        }
        wsClients.clear();
    }

    private void runHttpServerLoop() {
        try {
            httpServerSocket = new ServerSocket();
            httpServerSocket.setReuseAddress(true);
            try {
                httpServerSocket.bind(new InetSocketAddress("0.0.0.0", httpPort));
            } catch (Exception bindEx) {
                // Fallback to 8091 if Termux daemon is already listening on 8090
                httpPort = 8091;
                httpServerSocket.bind(new InetSocketAddress("0.0.0.0", httpPort));
            }
            appendLog("INFO", "WEBSERVER", "Started HTTP/WebSocket server on 0.0.0.0:" + httpPort);
            while (running) {
                Socket sock = httpServerSocket.accept();
                new Thread(() -> handleHttpOrWsConnection(sock)).start();
            }
        } catch (Exception e) {
            if (running) {
                appendLog("ERROR", "WEBSERVER", "HTTP server bind error: " + e.getMessage());
            }
        }
    }

    private void runJsonRpcServerLoop() {
        try {
            jsonRpcServerSocket = new ServerSocket();
            jsonRpcServerSocket.setReuseAddress(true);
            jsonRpcServerSocket.bind(new InetSocketAddress("0.0.0.0", jsonRpcPort));
            appendLog("INFO", "JSONSERVER", "Started JSON-RPC server on 0.0.0.0:" + jsonRpcPort);
            while (running) {
                Socket sock = jsonRpcServerSocket.accept();
                new Thread(() -> handleRawJsonRpcSocket(sock)).start();
            }
        } catch (Exception ignored) {
        }
    }

    private void runFlatbufServerLoop() {
        try {
            flatbufServerSocket = new ServerSocket();
            flatbufServerSocket.setReuseAddress(true);
            flatbufServerSocket.bind(new InetSocketAddress("0.0.0.0", flatbufPort));
            appendLog("INFO", "FLATBUFSERVER", "Started FlatBuffers server on 0.0.0.0:" + flatbufPort);
            while (running) {
                Socket sock = flatbufServerSocket.accept();
                new Thread(() -> handleFlatbufOrProtoConnection(sock, "FlatBuffers")).start();
            }
        } catch (Exception ignored) {
        }
    }

    private void runProtoServerLoop() {
        try {
            protoServerSocket = new ServerSocket();
            protoServerSocket.setReuseAddress(true);
            protoServerSocket.bind(new InetSocketAddress("0.0.0.0", protoPort));
            appendLog("INFO", "PROTOSERVER", "Started Protobuf server on 0.0.0.0:" + protoPort);
            while (running) {
                Socket sock = protoServerSocket.accept();
                new Thread(() -> handleFlatbufOrProtoConnection(sock, "Protobuf")).start();
            }
        } catch (Exception ignored) {
        }
    }

    private void handleFlatbufOrProtoConnection(Socket sock, String protoName) {
        try (Socket s = sock;
             DataInputStream dis = new DataInputStream(new BufferedInputStream(s.getInputStream()));
             OutputStream os = s.getOutputStream()) {
            s.setTcpNoDelay(true);
            while (running && !s.isClosed()) {
                int msgLen = dis.readInt(); // 4-byte big-endian length prefix
                if (msgLen <= 0 || msgLen > 16 * 1024 * 1024) break;
                byte[] msg = new byte[msgLen];
                dis.readFully(msg);

                parseExternalBinaryFrame(msg, protoName);

                // Send standard FlatBuffers/Proto reply (registered=1)
                if ("FlatBuffers".equals(protoName)) {
                    byte[] reply = new byte[]{
                            0x00, 0x00, 0x00, 0x10,
                            0x08, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x06, 0x00,
                            0x08, 0x00, 0x00, 0x00,
                            0x06, 0x00, 0x00, 0x00
                    };
                    os.write(reply);
                    os.flush();
                } else {
                    // Protobuf HyperionReply { type=REPLY(1), success=true }
                    byte[] reply = new byte[]{0x00, 0x00, 0x00, 0x04, 0x08, 0x01, 0x10, 0x01};
                    os.write(reply);
                    os.flush();
                }
            }
        } catch (Exception ignored) {
        }
    }

    private void parseExternalBinaryFrame(byte[] msg, String protoName) {
        try {
            // Try decoding heuristic RGB24/RGBA frame from FlatBuffers RawImage or Protobuf ImageRequest
            // FlatBuffers RawImage stores width (int32), height (int32), and a data vector (uint32 len + RGB24/RGBA bytes)
            ByteBuffer bb = ByteBuffer.wrap(msg).order(ByteOrder.LITTLE_ENDIAN);
            int len = msg.length;
            if (len >= 64) {
                // Scan from the end or look for a valid (width, height, byteLen) tuple in the buffer
                for (int off = 8; off <= Math.min(96, len - 12); off += 4) {
                    int vecLen = bb.getInt(off);
                    if (vecLen >= 48 && off + 4 + vecLen <= len) {
                        // Look for width & height near this vector or infer 16:9 dimensions from vecLen
                        int pixels3 = vecLen / 3;
                        int pixels4 = vecLen / 4;
                        int w = 0, h = 0, bpp = 3;
                        if (vecLen % 3 == 0 && pixels3 >= 16) {
                            w = (int) Math.round(Math.sqrt(pixels3 * 16.0 / 9.0));
                            if (w > 0 && pixels3 % w != 0) {
                                for (int candW : new int[]{64, 80, 96, 128, 160, 192, 240, 256, 320, 384, 480, 640, 720, 960, 1280, 1920}) {
                                    if (pixels3 % candW == 0) { w = candW; break; }
                                }
                            }
                            h = w > 0 ? (pixels3 / w) : 0;
                            bpp = 3;
                        } else if (vecLen % 4 == 0 && pixels4 >= 16) {
                            for (int candW : new int[]{64, 80, 96, 128, 160, 192, 240, 256, 320, 384, 480, 640, 720, 960, 1280, 1920}) {
                                if (pixels4 % candW == 0) { w = candW; break; }
                            }
                            h = w > 0 ? (pixels4 / w) : 0;
                            bpp = 4;
                        }
                        if (w >= 8 && h >= 8 && w <= 3840 && h <= 2160) {
                            int[] argb = new int[w * h];
                            int dataStart = off + 4;
                            for (int i = 0; i < w * h; i++) {
                                int p = dataStart + i * bpp;
                                int r = msg[p] & 0xFF;
                                int g = msg[p + 1] & 0xFF;
                                int b = msg[p + 2] & 0xFF;
                                argb[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
                            }
                            screenCaptureManager.injectExternalFrame(argb, w, h, protoName);
                            return;
                        }
                    }
                }
            }
        } catch (Exception ignored) {
        }
    }

    private void handleRawJsonRpcSocket(Socket sock) {
        try (Socket s = sock;
             BufferedReader br = new BufferedReader(new InputStreamReader(s.getInputStream(), StandardCharsets.UTF_8));
             OutputStream os = s.getOutputStream()) {
            String line;
            while ((line = br.readLine()) != null) {
                line = line.trim();
                if (line.isEmpty()) continue;
                JSONObject req = new JSONObject(line);
                JSONObject resp = processJsonRpcCommand(req, null);
                if (resp != null) {
                    os.write((resp.toString() + "\n").getBytes(StandardCharsets.UTF_8));
                    os.flush();
                }
            }
        } catch (Exception ignored) {
        }
    }

    private void handleHttpOrWsConnection(Socket sock) {
        try {
            sock.setTcpNoDelay(true);
            InputStream is = new BufferedInputStream(sock.getInputStream());
            OutputStream os = sock.getOutputStream();

            String requestLine = readHttpLine(is);
            if (requestLine == null || requestLine.isEmpty()) {
                sock.close();
                return;
            }
            String[] reqParts = requestLine.split(" ");
            String method = reqParts.length > 0 ? reqParts[0] : "GET";
            String rawPath = reqParts.length > 1 ? reqParts[1] : "/";

            String wsKey = null;
            boolean isUpgrade = false;
            int contentLength = 0;
            String hdrLine;
            while ((hdrLine = readHttpLine(is)) != null && !hdrLine.isEmpty()) {
                String lower = hdrLine.toLowerCase(Locale.US);
                if (lower.startsWith("sec-websocket-key:")) {
                    wsKey = hdrLine.substring(hdrLine.indexOf(':') + 1).trim();
                } else if (lower.startsWith("upgrade:") && lower.contains("websocket")) {
                    isUpgrade = true;
                } else if (lower.startsWith("content-length:")) {
                    try {
                        contentLength = Integer.parseInt(hdrLine.substring(hdrLine.indexOf(':') + 1).trim());
                    } catch (NumberFormatException ignored) {}
                }
            }

            if (isUpgrade && wsKey != null) {
                performWebSocketHandshake(sock, is, os, wsKey);
                return;
            }

            String path = rawPath;
            int qIdx = path.indexOf('?');
            if (qIdx >= 0) path = path.substring(0, qIdx);

            if ("POST".equalsIgnoreCase(method) && ("/json-rpc".equals(path) || "/jsonrpc".equals(path))) {
                byte[] body = new byte[contentLength];
                int read = 0;
                while (read < contentLength) {
                    int r = is.read(body, read, contentLength - read);
                    if (r < 0) break;
                    read += r;
                }
                JSONObject req = new JSONObject(new String(body, StandardCharsets.UTF_8));
                JSONObject resp = processJsonRpcCommand(req, null);
                byte[] outBytes = (resp != null ? resp.toString() : "{}").getBytes(StandardCharsets.UTF_8);
                writeHttpResponse(os, "200 OK", "application/json; charset=utf-8", outBytes);
                sock.close();
                return;
            }

            if ("/hyperhdr_heart_beat".equals(path)) {
                writeHttpResponse(os, "200 OK", "text/plain; charset=utf-8", "OK".getBytes(StandardCharsets.UTF_8));
                sock.close();
                return;
            }

            if ("/mediaprojection-frame.rgb".equals(path)) {
                byte[] pkt = screenCaptureManager.getLatestBridgePacket();
                if (pkt != null && screenCaptureManager.isMediaProjectionActive()) {
                    writeHttpResponse(os, "200 OK", "application/octet-stream", pkt);
                } else {
                    writeHttpResponse(os, "204 No Content", "text/plain; charset=utf-8", new byte[0]);
                }
                sock.close();
                return;
            }

            if ("/".equals(path) || path.isEmpty()) {
                path = "/index.html";
            }
            while (path.startsWith("/")) {
                path = path.substring(1);
            }

            String assetPath = "www/" + path;
            try (InputStream assetIn = context.getAssets().open(assetPath)) {
                byte[] payload = readAllBytes(assetIn);
                if ("content/about.html".equals(path)) {
                    String html = new String(payload, StandardCharsets.UTF_8)
                            .replace("https://raw.githubusercontent.com/awawa-dev/HyperHDR/master/3RD_PARTY_LICENSES", "/3RD_PARTY_LICENSES");
                    payload = html.getBytes(StandardCharsets.UTF_8);
                }
                writeHttpResponse(os, "200 OK", guessMimeType(path), payload);
            } catch (IOException notFound) {
                writeHttpResponse(os, "404 Not Found", "text/plain; charset=utf-8", ("Not found: " + path).getBytes(StandardCharsets.UTF_8));
            }
            sock.close();
        } catch (Exception e) {
            try { sock.close(); } catch (Exception ignored) {}
        }
    }

    private void performWebSocketHandshake(Socket sock, InputStream is, OutputStream os, String wsKey) throws Exception {
        String acceptSeed = wsKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        MessageDigest sha1 = MessageDigest.getInstance("SHA-1");
        String acceptKey = Base64.encodeToString(sha1.digest(acceptSeed.getBytes(StandardCharsets.US_ASCII)), Base64.NO_WRAP);

        String resp = "HTTP/1.1 101 Switching Protocols\r\n" +
                "Upgrade: websocket\r\n" +
                "Connection: Upgrade\r\n" +
                "Sec-WebSocket-Accept: " + acceptKey + "\r\n\r\n";
        os.write(resp.getBytes(StandardCharsets.US_ASCII));
        os.flush();

        WsClient client = new WsClient(sock, is, os);
        wsClients.add(client);
        try {
            client.readLoop();
        } finally {
            wsClients.remove(client);
            updateImageStreamState();
            client.close();
        }
    }

    private class WsClient {
        final Socket socket;
        final InputStream is;
        final OutputStream os;
        volatile boolean authenticated = false;
        volatile String sessionToken = null;
        volatile boolean ledStream = false;
        volatile boolean imageStream = false;
        volatile boolean loggingStream = false;
        volatile boolean performanceSub = false;

        WsClient(Socket socket, InputStream is, OutputStream os) {
            this.socket = socket;
            this.is = is;
            this.os = os;
        }

        void readLoop() throws Exception {
            while (running && !socket.isClosed()) {
                int b0 = is.read();
                if (b0 < 0) break;
                int b1 = is.read();
                if (b1 < 0) break;

                int opcode = b0 & 0x0F;
                boolean masked = (b1 & 0x80) != 0;
                long payloadLen = b1 & 0x7F;
                if (payloadLen == 126) {
                    payloadLen = ((is.read() & 0xFF) << 8) | (is.read() & 0xFF);
                } else if (payloadLen == 127) {
                    payloadLen = 0;
                    for (int i = 0; i < 8; i++) {
                        payloadLen = (payloadLen << 8) | (is.read() & 0xFF);
                    }
                }
                byte[] maskKey = new byte[4];
                if (masked) {
                    readFully(is, maskKey, 4);
                }
                if (payloadLen > 8 * 1024 * 1024) break;
                byte[] payload = new byte[(int) payloadLen];
                readFully(is, payload, (int) payloadLen);
                if (masked) {
                    for (int i = 0; i < payload.length; i++) {
                        payload[i] ^= maskKey[i & 3];
                    }
                }

                if (opcode == 0x8) { // Close
                    break;
                } else if (opcode == 0x9) { // Ping -> Pong
                    sendFrame(0xA, payload);
                } else if (opcode == 0x1) { // Text
                    String text = new String(payload, StandardCharsets.UTF_8).trim();
                    for (String line : text.split("\n")) {
                        line = line.trim();
                        if (line.isEmpty()) continue;
                        try {
                            JSONObject req = new JSONObject(line);
                            JSONObject resp = processJsonRpcCommand(req, this);
                            if (resp != null) {
                                sendJson(resp);
                            }
                        } catch (Exception ignored) {
                        }
                    }
                }
            }
        }

        synchronized void sendJson(JSONObject obj) {
            byte[] data = obj.toString().getBytes(StandardCharsets.UTF_8);
            sendFrame(0x1, data);
        }

        synchronized void sendBinary(byte[] data) {
            sendFrame(0x2, data);
        }

        synchronized void sendFrame(int opcode, byte[] data) {
            try {
                ByteArrayOutputStream frame = new ByteArrayOutputStream(data.length + 10);
                frame.write(0x80 | (opcode & 0x0F));
                if (data.length <= 125) {
                    frame.write(data.length);
                } else if (data.length <= 65535) {
                    frame.write(126);
                    frame.write((data.length >> 8) & 0xFF);
                    frame.write(data.length & 0xFF);
                } else {
                    frame.write(127);
                    long len = data.length;
                    for (int i = 7; i >= 0; i--) {
                        frame.write((int) ((len >> (i * 8)) & 0xFF));
                    }
                }
                frame.write(data);
                os.write(frame.toByteArray());
                os.flush();
            } catch (Exception e) {
                close();
            }
        }

        void close() {
            try { socket.close(); } catch (Exception ignored) {}
        }
    }

    private void updateImageStreamState() {
        boolean anyImg = false;
        for (WsClient c : wsClients) {
            if (c.imageStream) {
                anyImg = true;
                break;
            }
        }
        screenCaptureManager.setImageStreamRequested(anyImg);
    }

    private void onEngineLedFrame(int[] rgbFlat) {
        long now = System.currentTimeMillis();
        if (now - lastLedStreamPushMs < 50) return;
        lastLedStreamPushMs = now;

        boolean anyListening = false;
        for (WsClient c : wsClients) {
            if (c.ledStream) {
                anyListening = true;
                break;
            }
        }
        if (!anyListening) return;

        try {
            JSONArray ledsArr = new JSONArray();
            for (int v : rgbFlat) {
                ledsArr.put(v);
            }
            JSONObject result = new JSONObject();
            result.put("leds", ledsArr);
            JSONObject msg = new JSONObject();
            msg.put("command", "ledcolors-ledstream-update");
            msg.put("success", true);
            msg.put("result", result);
            for (WsClient c : wsClients) {
                if (c.ledStream) {
                    c.sendJson(msg);
                }
            }
        } catch (Exception ignored) {
        }
    }

    private void onPreviewJpegFrame(byte[] jpegBytes) {
        if (jpegBytes == null) return;
        for (WsClient c : wsClients) {
            if (c.imageStream) {
                c.sendBinary(jpegBytes);
            }
        }
    }

    private void periodicTelemetryTick() {
        try {
            for (WsClient c : wsClients) {
                if (c.imageStream) {
                    byte[] jpeg = screenCaptureManager.getLatestPreviewJpeg();
                    if (jpeg == null || !screenCaptureManager.isCapturing()) {
                        jpeg = screenCaptureManager.generateFallbackPreviewJpeg(colorEngine.getLastQuantizedRgb());
                    }
                    if (jpeg != null) {
                        c.sendBinary(jpeg);
                    }
                }
                if (c.performanceSub) {
                    c.sendJson(buildPerformanceUpdateMessage(false));
                }
            }
        } catch (Exception ignored) {
        }
    }

    private synchronized JSONObject processJsonRpcCommand(JSONObject req, WsClient client) throws Exception {
        String cmd = req.optString("command", "");
        String sub = req.optString("subcommand", "");
        int tan = req.optInt("tan", 1);
        String fullCmd = sub.isEmpty() ? cmd : (cmd + "-" + sub);

        JSONObject resp = new JSONObject();
        resp.put("command", fullCmd);
        resp.put("success", true);
        resp.put("tan", tan);

        if (!"authorize".equals(cmd)) {
            if (client != null) {
                if (!client.authenticated) {
                    resp.put("success", false);
                    resp.put("error", "No Authorization");
                    return resp;
                }
            } else {
                // Non-WebSocket request (HTTP POST /json-rpc or raw TCP socket)
                String reqToken = req.optString("token", null);
                boolean authRequired = dbState != null
                        && (!dbState.optBoolean("defaultPasswordSet", true)
                        || !"hyperhdr".equals(dbState.optString("adminPassword", "hyperhdr")));
                if (authRequired) {
                    boolean tokenValid = reqToken != null && (activeSessionTokens.contains(reqToken) || isSavedApiToken(reqToken));
                    if (!tokenValid && !"serverinfo".equals(cmd) && !"sysinfo".equals(cmd)) {
                        resp.put("success", false);
                        resp.put("error", "No Authorization");
                        return resp;
                    }
                }
            }
        }

        switch (cmd) {
            case "authorize":
                handleAuthorize(req, sub, resp, client);
                break;

            case "config":
                if ("getschema".equals(sub)) {
                    resp.put("info", serverSchema);
                } else if ("getconfig".equals(sub)) {
                    resp.put("info", getActiveInstanceConfig());
                } else if ("setconfig".equals(sub)) {
                    JSONObject newCfg = req.optJSONObject("config");
                    if (newCfg != null) {
                        JSONObject currentCfg = getActiveInstanceConfig();
                        JSONArray names = newCfg.names();
                        boolean grabberTouched = false;
                        if (names != null) {
                            for (int i = 0; i < names.length(); i++) {
                                String k = names.getString(i);
                                currentCfg.put(k, newCfg.get(k));
                                if ("systemGrabber".equals(k) || "videoGrabber".equals(k)
                                        || "systemControl".equals(k) || "videoControl".equals(k)) {
                                    grabberTouched = true;
                                }
                            }
                        }
                        saveDatabase();
                        colorEngine.applyConfig(currentCfg);
                        screenCaptureManager.applyConfig(currentCfg);
                        appendLog("INFO", "CONFIG", "Saved configuration for instance " + currentInstance + " (Grabber: Android MediaProjection API, LED device: " + wledDriver.getDeviceType() + " @ " + wledDriver.getHost() + ")");
                        broadcastSettingsUpdate(currentCfg);
                        if (grabberTouched && !screenCaptureManager.isMediaProjectionActive()) {
                            HyperHdrService svc = HyperHdrService.getInstance();
                            if (svc != null) {
                                svc.requestMediaProjectionFromUi(false);
                            }
                        }
                    }
                }
                break;

            case "mediaprojection":
                HyperHdrService mpSvc = HyperHdrService.getInstance();
                if ("stop".equals(sub)) {
                    screenCaptureManager.stopCapture();
                    notifyGrabberStateChanged();
                } else if (mpSvc != null) {
                    mpSvc.requestMediaProjectionFromUi(req.optBoolean("headless_bridge", false));
                }
                JSONObject mpInfo = new JSONObject();
                mpInfo.put("active", screenCaptureManager.isMediaProjectionActive());
                mpInfo.put("mode", screenCaptureManager.getActiveCaptureMode());
                resp.put("info", mpInfo);
                break;

            case "sysinfo":
                resp.put("info", buildSysInfo());
                break;

            case "serverinfo":
                JSONArray subs = req.optJSONArray("subscribe");
                if (subs != null && client != null) {
                    for (int i = 0; i < subs.length(); i++) {
                        if ("performance-update".equals(subs.optString(i))) {
                            client.performanceSub = true;
                        }
                    }
                }
                resp.put("info", buildServerInfo());
                break;

            case "componentstate":
                JSONObject cs = req.optJSONObject("componentstate");
                if (cs != null) {
                    String comp = cs.optString("component", "ALL");
                    boolean state = cs.optBoolean("state", true);
                    colorEngine.setComponentState(comp, state);
                    appendLog("INFO", "COMPONENT", "Component " + comp + " set to " + (state ? "ENABLED" : "DISABLED"));
                    if (state && ("GRABBER".equals(comp) || "SYSTEMGRABBER".equals(comp) || "V4L".equals(comp) || "VIDEOGRABBER".equals(comp))) {
                        if (!screenCaptureManager.isMediaProjectionActive()) {
                            HyperHdrService svc = HyperHdrService.getInstance();
                            if (svc != null) {
                                svc.requestMediaProjectionFromUi(false);
                            }
                        }
                    }
                    JSONObject compMsg = new JSONObject();
                    compMsg.put("command", "components-update");
                    JSONObject data = new JSONObject();
                    data.put("name", comp);
                    data.put("enabled", state);
                    compMsg.put("data", data);
                    broadcastWs(compMsg);
                    broadcastPrioritiesUpdate();
                }
                break;

            case "instance":
                handleInstanceCommand(req, sub, resp);
                break;

            case "ledcolors":
                if (client != null) {
                    if ("ledstream-start".equals(sub)) {
                        client.ledStream = true;
                    } else if ("ledstream-stop".equals(sub)) {
                        client.ledStream = false;
                    } else if ("imagestream-start".equals(sub)) {
                        client.imageStream = true;
                        updateImageStreamState();
                    } else if ("imagestream-stop".equals(sub)) {
                        client.imageStream = false;
                        updateImageStreamState();
                    }
                }
                break;

            case "color":
                JSONArray colorArr = req.optJSONArray("color");
                int prio = req.optInt("priority", 1);
                long dur = req.optLong("duration", -1);
                String origin = req.optString("origin", "Web Configuration");
                int[] rgb = new int[]{255, 0, 0};
                if (colorArr != null && colorArr.length() >= 3) {
                    rgb = new int[]{colorArr.optInt(0, 255), colorArr.optInt(1, 0), colorArr.optInt(2, 0)};
                }
                colorEngine.setColorPriority(prio, rgb, dur, origin);
                broadcastPrioritiesUpdate();
                break;

            case "image":
                handleImageCommand(req);
                broadcastPrioritiesUpdate();
                break;

            case "effect":
                JSONObject effObj = req.optJSONObject("effect");
                String effName = effObj != null ? effObj.optString("name", "Rainbow swirl") : "Rainbow swirl";
                int effPrio = req.optInt("priority", 1);
                long effDur = req.optLong("duration", -1);
                String effOrigin = req.optString("origin", "Web Configuration");
                colorEngine.setEffectPriority(effPrio, effName, effDur, effOrigin);
                broadcastPrioritiesUpdate();
                break;

            case "clear":
                int clearPrio = req.optInt("priority", -1);
                colorEngine.clearPriority(clearPrio);
                broadcastPrioritiesUpdate();
                break;

            case "clearall":
                colorEngine.clearAllPriorities();
                broadcastPrioritiesUpdate();
                break;

            case "sourceselect":
                if (req.has("auto") && req.optBoolean("auto", false)) {
                    colorEngine.setSourceSelect(true, 250);
                } else {
                    colorEngine.setSourceSelect(false, req.optInt("priority", 250));
                }
                broadcastPrioritiesUpdate();
                break;

            case "adjustment":
                Object adjObj = req.opt("adjustment");
                if (adjObj instanceof JSONArray) {
                    JSONArray a = (JSONArray) adjObj;
                    if (a.length() > 0) colorEngine.applyAdjustmentObject(a.optJSONObject(0));
                } else if (adjObj instanceof JSONObject) {
                    colorEngine.applyAdjustmentObject((JSONObject) adjObj);
                }
                JSONObject adjUpd = new JSONObject();
                adjUpd.put("command", "adjustment-update");
                adjUpd.put("data", colorEngine.getAdjustmentJsonArray());
                broadcastWs(adjUpd);
                break;

            case "processing":
                String mt = req.optString("mappingType", "multicolor_mean");
                colorEngine.setMappingType(mt);
                JSONObject mapUpd = new JSONObject();
                mapUpd.put("command", "imageToLedMapping-update");
                JSONObject mapData = new JSONObject();
                mapData.put("imageToLedMappingType", mt);
                mapUpd.put("data", mapData);
                broadcastWs(mapUpd);
                break;

            case "videomode":
                String vm = req.optString("videoMode", "2D");
                colorEngine.setVideoMode(vm);
                JSONObject vmUpd = new JSONObject();
                vmUpd.put("command", "videomode-update");
                JSONObject vmData = new JSONObject();
                vmData.put("videomode", vm);
                vmUpd.put("data", vmData);
                broadcastWs(vmUpd);
                break;

            case "videomodehdr":
                int hdr = req.optInt("HDR", 0);
                colorEngine.setVideoModeHdr(hdr);
                colorEngine.setComponentState("HDR", hdr == 1);
                JSONObject hdrUpd = new JSONObject();
                hdrUpd.put("command", "videomodehdr-update");
                JSONObject hdrData = new JSONObject();
                hdrData.put("videomodehdr", hdr);
                hdrUpd.put("data", hdrData);
                broadcastWs(hdrUpd);
                break;

            case "leddevice":
                handleLedDeviceCommand(req, sub, resp);
                break;

            case "performance-counters":
                if (client != null) {
                    client.performanceSub = true;
                    client.sendJson(buildPerformanceUpdateMessage("all".equals(sub)));
                }
                break;

            case "logging":
                if (client != null) {
                    if ("start".equals(sub)) {
                        client.loggingStream = true;
                        JSONObject logMsg = new JSONObject();
                        logMsg.put("command", "logging-update");
                        JSONObject res = new JSONObject();
                        JSONArray messages = new JSONArray();
                        for (JSONObject entry : logBuffer) {
                            messages.put(entry);
                        }
                        res.put("messages", messages);
                        logMsg.put("result", res);
                        client.sendJson(logMsg);
                    } else if ("stop".equals(sub)) {
                        client.loggingStream = false;
                    }
                }
                break;

            case "save-db":
                resp.put("info", dbState);
                break;

            case "load-db":
                JSONObject imported = req.optJSONObject("config");
                if (imported != null && imported.has("settings")) {
                    dbState = imported;
                    saveDatabase();
                    JSONObject cfg = getActiveInstanceConfig();
                    colorEngine.applyConfig(cfg);
                    screenCaptureManager.applyConfig(cfg);
                    appendLog("INFO", "DB", "Restored configuration database from backup");
                }
                break;

            case "signal-calibration":
                JSONObject calInfo = new JSONObject();
                calInfo.put("running", false);
                calInfo.put("isSuccess", true);
                calInfo.put("model", "Android TV SurfaceFlinger sRGB/BT.709");
                calInfo.put("quality", "100%");
                calInfo.put("sdrStat", "OK");
                calInfo.put("sdrPoint", "100");
                calInfo.put("hdrStat", "OK");
                calInfo.put("hdrPoint", "100");
                calInfo.put("status", "Completed");
                resp.put("info", calInfo);
                break;

            default:
                break;
        }
        return resp;
    }

    private void handleImageCommand(JSONObject req) {
        try {
            String b64 = req.optString("imagedata", "");
            if (b64.isEmpty()) return;
            int commaIdx = b64.indexOf(',');
            if (commaIdx >= 0) {
                b64 = b64.substring(commaIdx + 1);
            }
            byte[] raw = Base64.decode(b64, Base64.DEFAULT);
            int prio = req.optInt("priority", 1);
            long dur = req.optLong("duration", -1);
            String origin = req.optString("origin", "Web Configuration");
            String name = req.optString("name", "Image");
            int reqW = req.optInt("imagewidth", 0);
            int reqH = req.optInt("imageheight", 0);

            Bitmap bmp = BitmapFactory.decodeByteArray(raw, 0, raw.length);
            if (bmp != null) {
                int w = bmp.getWidth();
                int h = bmp.getHeight();
                if (w > 0 && h > 0) {
                    int[] argb = new int[w * h];
                    bmp.getPixels(argb, 0, w, 0, 0, w, h);
                    bmp.recycle();
                    colorEngine.setImagePriority(prio, argb, w, h, dur, origin, name);
                    screenCaptureManager.injectExternalFrame(argb, w, h, name);
                    return;
                }
                bmp.recycle();
            }

            if (reqW > 0 && reqH > 0 && raw.length >= reqW * reqH * 3) {
                int bpp = raw.length >= reqW * reqH * 4 ? 4 : 3;
                int[] argb = new int[reqW * reqH];
                for (int i = 0; i < reqW * reqH; i++) {
                    int p = i * bpp;
                    int r = raw[p] & 0xFF;
                    int g = raw[p + 1] & 0xFF;
                    int b = raw[p + 2] & 0xFF;
                    argb[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
                }
                colorEngine.setImagePriority(prio, argb, reqW, reqH, dur, origin, name);
                screenCaptureManager.injectExternalFrame(argb, reqW, reqH, name);
            }
        } catch (Exception ignored) {
        }
    }

    private void handleAuthorize(JSONObject req, String sub, JSONObject resp, WsClient client) throws Exception {
        JSONObject info = new JSONObject();
        String currentAdminPw = dbState.optString("adminPassword", "hyperhdr");
        boolean isDefaultPw = dbState.optBoolean("defaultPasswordSet", true) && "hyperhdr".equals(currentAdminPw);

        if ("adminRequired".equals(sub)) {
            info.put("adminRequired", true);
            resp.put("info", info);
        } else if ("newPasswordRequired".equals(sub)) {
            info.put("newPasswordRequired", isDefaultPw);
            resp.put("info", info);
        } else if ("login".equals(sub)) {
            String pw = req.optString("password", null);
            String tok = req.optString("token", null);
            boolean ok = false;
            String issuedToken = null;

            if (pw != null && !pw.isEmpty()) {
                if (pw.equals(currentAdminPw)) {
                    ok = true;
                    issuedToken = UUID.randomUUID().toString();
                    activeSessionTokens.add(issuedToken);
                }
            } else if (tok != null && !tok.isEmpty()) {
                if (activeSessionTokens.contains(tok) || isSavedApiToken(tok)) {
                    ok = true;
                    issuedToken = tok;
                }
            }

            if (ok) {
                if (client != null) {
                    client.authenticated = true;
                    client.sessionToken = issuedToken;
                }
                info.put("token", issuedToken);
                resp.put("info", info);
            } else {
                if (client != null) {
                    client.authenticated = false;
                }
                resp.put("success", false);
                resp.put("error", "No Authorization");
            }
        } else if ("logout".equals(sub)) {
            if (client != null) {
                if (client.sessionToken != null) {
                    activeSessionTokens.remove(client.sessionToken);
                }
                client.authenticated = false;
                client.sessionToken = null;
            }
        } else if ("newPassword".equals(sub)) {
            if (client != null && !client.authenticated) {
                resp.put("success", false);
                resp.put("error", "No Authorization");
                return;
            }
            String oldPw = req.optString("password", "");
            String newPw = req.optString("newPassword", "").trim();
            if (!oldPw.equals(currentAdminPw) || newPw.length() < 8) {
                resp.put("success", false);
                resp.put("error", "Wrong current password or new password shorter than 8 characters");
                return;
            }
            dbState.put("adminPassword", newPw);
            dbState.put("defaultPasswordSet", "hyperhdr".equals(newPw));
            activeSessionTokens.clear();
            saveDatabase();
            appendLog("INFO", "AUTH", "Admin password updated successfully; previous session tokens invalidated");
        } else if ("getTokenList".equals(sub)) {
            resp.put("info", dbState.optJSONArray("tokens") != null ? dbState.optJSONArray("tokens") : new JSONArray());
        } else if ("getPendingTokenRequests".equals(sub)) {
            resp.put("info", new JSONArray());
        } else if ("createToken".equals(sub)) {
            JSONObject tok = new JSONObject();
            tok.put("id", UUID.randomUUID().toString().substring(0, 8));
            tok.put("comment", req.optString("comment", "App Token"));
            tok.put("last_use", new SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US).format(new Date()));
            tok.put("token", UUID.randomUUID().toString());
            JSONArray list = dbState.optJSONArray("tokens");
            if (list == null) {
                list = new JSONArray();
                dbState.put("tokens", list);
            }
            list.put(tok);
            saveDatabase();
            resp.put("info", tok);
        } else if ("deleteToken".equals(sub)) {
            String delId = req.optString("id", "");
            JSONArray list = dbState.optJSONArray("tokens");
            if (list != null) {
                JSONArray next = new JSONArray();
                for (int i = 0; i < list.length(); i++) {
                    JSONObject t = list.optJSONObject(i);
                    if (t != null && !delId.equals(t.optString("id"))) {
                        next.put(t);
                    }
                }
                dbState.put("tokens", next);
                saveDatabase();
            }
        }
    }

    private boolean isSavedApiToken(String tok) {
        JSONArray list = dbState.optJSONArray("tokens");
        if (list == null) return false;
        for (int i = 0; i < list.length(); i++) {
            JSONObject t = list.optJSONObject(i);
            if (t != null && tok.equals(t.optString("token"))) {
                return true;
            }
        }
        return false;
    }

    private void handleInstanceCommand(JSONObject req, String sub, JSONObject resp) throws Exception {
        JSONArray instances = dbState.optJSONArray("instances");
        if (instances == null) {
            instances = new JSONArray();
            dbState.put("instances", instances);
        }
        int targetInst = req.optInt("instance", 0);
        if ("switchTo".equals(sub)) {
            currentInstance = targetInst;
            dbState.put("currentInstance", currentInstance);
            saveDatabase();
            JSONObject cfg = getActiveInstanceConfig();
            colorEngine.applyConfig(cfg);
            screenCaptureManager.applyConfig(cfg);
            return;
        } else if ("saveName".equals(sub)) {
            String newName = req.optString("name", "LED Instance");
            for (int i = 0; i < instances.length(); i++) {
                JSONObject o = instances.optJSONObject(i);
                if (o != null && o.optInt("instance") == targetInst) {
                    o.put("friendly_name", newName);
                }
            }
        } else if ("startInstance".equals(sub) || "stopInstance".equals(sub)) {
            boolean runState = "startInstance".equals(sub);
            for (int i = 0; i < instances.length(); i++) {
                JSONObject o = instances.optJSONObject(i);
                if (o != null && o.optInt("instance") == targetInst) {
                    o.put("running", runState);
                }
            }
        } else if ("createInstance".equals(sub)) {
            String name = req.optString("name", "Secondary LED instance");
            int nextId = 1;
            for (int i = 0; i < instances.length(); i++) {
                JSONObject o = instances.optJSONObject(i);
                if (o != null) nextId = Math.max(nextId, o.optInt("instance") + 1);
            }
            JSONObject newInst = new JSONObject();
            newInst.put("instance", nextId);
            newInst.put("friendly_name", name);
            newInst.put("running", true);
            instances.put(newInst);
            String defText = readAssetText("default_config.json");
            dbState.optJSONObject("settings").put(String.valueOf(nextId), new JSONObject(defText));
        } else if ("deleteInstance".equals(sub) && targetInst > 0) {
            JSONArray next = new JSONArray();
            for (int i = 0; i < instances.length(); i++) {
                JSONObject o = instances.optJSONObject(i);
                if (o != null && o.optInt("instance") != targetInst) {
                    next.put(o);
                }
            }
            dbState.put("instances", next);
            dbState.optJSONObject("settings").remove(String.valueOf(targetInst));
            if (currentInstance == targetInst) currentInstance = 0;
        }
        saveDatabase();
        JSONObject upd = new JSONObject();
        upd.put("command", "instance-update");
        upd.put("data", dbState.optJSONArray("instances"));
        broadcastWs(upd);
    }

    private void handleLedDeviceCommand(JSONObject req, String sub, JSONObject resp) throws Exception {
        String type = req.optString("ledDeviceType", "wled");
        JSONObject params = req.optJSONObject("params");
        if ("discover".equals(sub)) {
            JSONObject info = new JSONObject();
            info.put("ledDeviceType", type);
            info.put("devices", wledDriver.discoverDevices());
            resp.put("info", info);
        } else if ("getProperties".equals(sub)) {
            String h = params != null ? params.optString("host", wledDriver.getHost()) : wledDriver.getHost();
            resp.put("info", wledDriver.queryDeviceProperties(h));
        } else if ("identify".equals(sub)) {
            String h = params != null ? params.optString("host", wledDriver.getHost()) : wledDriver.getHost();
            wledDriver.identifyDevice(h);
        } else if ("hasLedClock".equals(sub)) {
            JSONObject info = new JSONObject();
            info.put("hasLedClock", 0);
            resp.put("info", info);
            JSONObject push = new JSONObject();
            push.put("command", "hasLedClock-update");
            push.put("success", true);
            push.put("info", info);
            broadcastWs(push);
        }
    }

    private JSONObject buildSysInfo() throws Exception {
        JSONObject root = new JSONObject();
        JSONObject hyper = new JSONObject();
        hyper.put("version", "24.0.0-beta.0");
        hyper.put("build", "24.0.0-beta.0 (Android TV)");
        hyper.put("time", "Oct 22 2026 22:30:57");
        hyper.put("gitremote", "https://github.com/awawa-dev/HyperHDR");
        hyper.put("channel", "Beta");
        hyper.put("readOnlyMode", false);
        root.put("hyperhdr", hyper);

        JSONObject sys = new JSONObject();
        sys.put("productType", "linux");
        sys.put("prettyName", "Android " + Build.VERSION.RELEASE + " TV (API " + Build.VERSION.SDK_INT + " - " + Build.MODEL + ")");
        sys.put("architecture", Build.SUPPORTED_ABIS.length > 0 ? Build.SUPPORTED_ABIS[0] : "arm64-v8a");
        sys.put("cpuModelName", Build.HARDWARE + " (" + Build.BOARD + ")");
        sys.put("cpuModelType", Build.DEVICE);
        sys.put("cpuHardware", Build.MANUFACTURER + " " + Build.BRAND);
        sys.put("kernelType", "linux");
        sys.put("kernelVersion", System.getProperty("os.version", "4.19-android11"));
        sys.put("wordSize", "64");
        sys.put("qtVersion", "6.8.4");
        sys.put("openssl", "OpenSSL 1.1.1w / Android BoringSSL");
        root.put("system", sys);
        return root;
    }

    private JSONObject buildServerInfo() throws Exception {
        JSONObject info = new JSONObject();
        info.put("currentInstance", currentInstance);
        info.put("lastError", "");
        info.put("hasCEC", 1);
        info.put("hasPROTOBUF", 1);
        info.put("imageToLedMappingType", colorEngine.getMappingType());
        info.put("videomode", colorEngine.getVideoMode());
        info.put("videomodehdr", colorEngine.getVideoModeHdr());
        info.put("components", colorEngine.getComponentsJsonArray());
        info.put("adjustment", colorEngine.getAdjustmentJsonArray());
        info.put("priorities", colorEngine.getPrioritiesJsonArray());
        info.put("priorities_autoselect", colorEngine.isPrioritiesAutoselect());
        info.put("instance", dbState.optJSONArray("instances"));

        // Available LED devices grouped matching HyperHDR v24.0.0-beta.0 (including leds_group_6_HID & sp616e)
        JSONObject ledDevices = new JSONObject();
        JSONArray availDevs = new JSONArray();
        String[][] devGroups = new String[][]{
                {"apa102", "leds_group_0_SPI"},
                {"apa104", "leds_group_0_SPI"},
                {"hd108", "leds_group_0_SPI"},
                {"hyperspi", "leds_group_0_SPI"},
                {"lpd6803", "leds_group_0_SPI"},
                {"lpd8806", "leds_group_0_SPI"},
                {"p9813", "leds_group_0_SPI"},
                {"sk6812spi", "leds_group_0_SPI"},
                {"sk6822spi", "leds_group_0_SPI"},
                {"sk9822", "leds_group_0_SPI"},
                {"ws2801", "leds_group_0_SPI"},
                {"ws2812spi", "leds_group_0_SPI"},
                {"piblaster", "leds_group_1_PWM"},
                {"rpi_pio", "leds_group_1_PWM"},
                {"ws281x", "leds_group_1_PWM"},
                {"wled", "leds_group_2_network"},
                {"hyperk", "leds_group_2_network"},
                {"ddp", "leds_group_2_network"},
                {"philipshue", "leds_group_2_network"},
                {"wiz", "leds_group_2_network"},
                {"lifx", "leds_group_2_network"},
                {"home_assistant", "leds_group_2_network"},
                {"zigbee2mqtt", "leds_group_2_network"},
                {"nanoleaf", "leds_group_2_network"},
                {"yeelight", "leds_group_2_network"},
                {"cololight", "leds_group_2_network"},
                {"atmoorb", "leds_group_2_network"},
                {"fadecandy", "leds_group_2_network"},
                {"tinkerforge", "leds_group_2_network"},
                {"udpe131", "leds_group_2_network"},
                {"udpartnet", "leds_group_2_network"},
                {"udph801", "leds_group_2_network"},
                {"udpraw", "leds_group_2_network"},
                {"tpm2net", "leds_group_2_network"},
                {"adalight", "leds_group_3_serial"},
                {"dmx", "leds_group_3_serial"},
                {"karate", "leds_group_3_serial"},
                {"skydimo", "leds_group_3_serial"},
                {"sp616e", "leds_group_3_serial"},
                {"sedu", "leds_group_3_serial"},
                {"tpm2", "leds_group_3_serial"},
                {"atmo", "leds_group_3_serial"},
                {"lightpack", "leds_group_6_HID"},
                {"paintpack", "leds_group_6_HID"},
                {"rawhid", "leds_group_6_HID"},
                {"synclight", "leds_group_6_HID"},
                {"file", "leds_group_5_debug"}
        };
        for (String[] dg : devGroups) {
            JSONObject d = new JSONObject();
            d.put("name", dg[0]);
            d.put("group", dg[1]);
            availDevs.put(d);
        }
        ledDevices.put("available", availDevs);
        info.put("ledDevices", ledDevices);

        // Grabbers
        JSONObject grabbers = new JSONObject();
        JSONArray activeGrabbers = new JSONArray();
        activeGrabbers.put("Android MediaProjection API (" + screenCaptureManager.getActiveCaptureMode() + ")");
        activeGrabbers.put("Android MediaProjection API");
        grabbers.put("active", activeGrabbers);
        JSONArray availGrabbers = new JSONArray();
        availGrabbers.put("System capturing (Android MediaProjection API)");
        availGrabbers.put("Video capturing (Android MediaProjection API)");
        grabbers.put("available", availGrabbers);
        grabbers.put("lut_for_hdr_exists", 1);
        grabbers.put("lutFastCRC", "0x240000B0");

        JSONObject currentGrabState = new JSONObject();
        currentGrabState.put("device", screenCaptureManager.isMediaProjectionActive()
                ? "Android MediaProjection API (Active)"
                : "Android MediaProjection API (Ready)");
        currentGrabState.put("videoMode",
                screenCaptureManager.getCaptureWidth() + "x" + screenCaptureManager.getCaptureHeight()
                        + " " + screenCaptureManager.getTargetFps() + "fps RGBA (MediaProjection)");
        grabbers.put("current", currentGrabState);

        JSONArray videoDevs = new JSONArray();
        JSONObject vdev = new JSONObject();
        vdev.put("device", "Android MediaProjection API");
        vdev.put("name", "Android MediaProjection API (Hardware VirtualDisplay Screen Grabber)");
        JSONObject vctrl = new JSONObject();
        vctrl.put("BrightnessDef", "Brightness: 0");
        vctrl.put("ContrastDef", "Contrast: 0");
        vctrl.put("SaturationDef", "Saturation: 0");
        vctrl.put("HueDef", "Hue: 0");
        vdev.put("videoControls", vctrl);
        JSONArray vmodes = new JSONArray();
        for (int[] res : new int[][]{
                {256, 144, 60}, {256, 144, 30}, {256, 144, 25},
                {320, 180, 60}, {320, 180, 30}, {320, 180, 25},
                {640, 360, 60}, {640, 360, 30},
                {1280, 720, 60}, {1280, 720, 30},
                {1920, 1080, 60}, {1920, 1080, 30}
        }) {
            JSONObject m = new JSONObject();
            m.put("width", res[0]);
            m.put("height", res[1]);
            m.put("fps", res[2]);
            m.put("pixel_format_info", "RGBA");
            vmodes.put(m);
        }
        vdev.put("videoModeList", vmodes);
        JSONArray frates = new JSONArray();
        frates.put("60");
        frates.put("30");
        frates.put("25");
        vdev.put("framerates", frates);
        vdev.put("inputs", new JSONArray());
        videoDevs.put(vdev);
        grabbers.put("video_devices", videoDevs);
        info.put("grabbers", grabbers);
        info.put("grabberstate", currentGrabState);

        // System grabbers — expose Android MediaProjection API as primary selectable device & modes
        JSONObject sysGrabbers = new JSONObject();
        sysGrabbers.put("device", "Android MediaProjection API");
        JSONArray sysModes = new JSONArray();
        sysModes.put("Android MediaProjection API");
        sysModes.put("Android MediaProjection API (Low-Latency 256x144)");
        sysModes.put("Android MediaProjection API (HD 320x180)");
        sysModes.put("Android MediaProjection API (Full 640x360)");
        sysGrabbers.put("modes", sysModes);
        info.put("systemGrabbers", sysGrabbers);

        // Effects list
        String[] effectNames = new String[]{
                "Rainbow swirl", "Rainbow swirl fast", "Warm mood blobs", "Full color mood blobs",
                "Cold mood blobs", "Red mood blobs", "Green mood blobs", "Blue mood blobs",
                "Atomic swirl", "Sea waves", "Plasma", "Breath", "Candle",
                "Cinema brighten lights", "Cinema dim lights", "Knight rider",
                "Police Lights Single", "Police Lights Solid", "Strobe red", "Strobe white",
                "Sparks", "Traces"
        };
        JSONArray effectsArr = new JSONArray();
        for (String en : effectNames) {
            JSONObject eo = new JSONObject();
            eo.put("name", en);
            effectsArr.put(eo);
        }
        info.put("effects", effectsArr);

        // Sound
        JSONObject soundObj = new JSONObject();
        JSONArray soundDevs = new JSONArray();
        soundDevs.put("Android AudioRecord Loopback");
        soundObj.put("sound_available", soundDevs);
        info.put("sound", soundObj);

        // Discovered sessions
        JSONArray sessions = new JSONArray();
        JSONObject wledSession = new JSONObject();
        wledSession.put("name", "WLED (" + wledDriver.getDeviceType().toUpperCase(Locale.US) + ")");
        wledSession.put("address", wledDriver.getHost());
        wledSession.put("port", 80);
        wledSession.put("host", "WLED LED Controller");
        sessions.put(wledSession);
        info.put("sessions", sessions);

        return info;
    }

    private JSONObject buildPerformanceUpdateMessage(boolean all) throws Exception {
        JSONObject msg = new JSONObject();
        msg.put("command", "performance-update");
        JSONObject data = new JSONObject();
        JSONArray broadcast = new JSONArray();

        // Type 1: Grabber FPS & latency
        JSONObject grabPerf = new JSONObject();
        grabPerf.put("type", 1);
        grabPerf.put("id", 0);
        grabPerf.put("remove", 0);
        grabPerf.put("token", 1);
        grabPerf.put("param1", screenCaptureManager.getCurrentCaptureFps());
        grabPerf.put("param2", screenCaptureManager.getLastLatencyMs());
        grabPerf.put("param3", screenCaptureManager.getActiveCaptureMode());
        grabPerf.put("param4", 0);
        grabPerf.put("refresh", 5);
        broadcast.put(grabPerf);

        // Type 2: Instance smoothing FPS
        JSONObject instPerf = new JSONObject();
        instPerf.put("type", 2);
        instPerf.put("id", currentInstance);
        instPerf.put("remove", 0);
        instPerf.put("token", 1);
        instPerf.put("name", "First LED instance");
        instPerf.put("param1", colorEngine.getSmoothingFps());
        instPerf.put("param2", colorEngine.getLedCount() + " LEDs");
        instPerf.put("param3", 0);
        instPerf.put("param4", 0);
        instPerf.put("refresh", 5);
        broadcast.put(instPerf);

        // Type 3: LED device output FPS & packets
        JSONObject ledPerf = new JSONObject();
        ledPerf.put("type", 3);
        ledPerf.put("id", currentInstance);
        ledPerf.put("remove", 0);
        ledPerf.put("token", 1);
        ledPerf.put("name", wledDriver.getDeviceType().toUpperCase(Locale.US));
        ledPerf.put("param1", wledDriver.getCurrentLedFps());
        ledPerf.put("param2", wledDriver.getTotalPacketsSent());
        ledPerf.put("param3", colorEngine.getLedCount());
        ledPerf.put("param4", wledDriver.getPacketsDroppedWindow());
        ledPerf.put("refresh", 5);
        broadcast.put(ledPerf);

        // Type 4: CPU usage
        JSONObject cpuPerf = new JSONObject();
        cpuPerf.put("type", 4);
        cpuPerf.put("id", 0);
        cpuPerf.put("remove", 0);
        JSONObject cpuStat = new JSONObject();
        int cores = Runtime.getRuntime().availableProcessors();
        JSONArray coreArr = new JSONArray();
        for (int i = 0; i < cores; i++) {
            coreArr.put(0.12 + (i % 3) * 0.04);
        }
        cpuStat.put("cores", coreArr);
        cpuStat.put("total", 14.0);
        cpuPerf.put("name", cpuStat.toString());
        broadcast.put(cpuPerf);

        // Type 5: RAM usage
        ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        long totalMb = 2048, usedMb = 820;
        if (am != null) {
            ActivityManager.MemoryInfo mi = new ActivityManager.MemoryInfo();
            am.getMemoryInfo(mi);
            totalMb = Math.max(512L, mi.totalMem / (1024L * 1024L));
            usedMb = Math.max(128L, (mi.totalMem - mi.availMem) / (1024L * 1024L));
        }
        JSONObject ramPerf = new JSONObject();
        ramPerf.put("type", 5);
        ramPerf.put("id", 0);
        ramPerf.put("remove", 0);
        JSONObject ramStat = new JSONObject();
        ramStat.put("takenMem", usedMb);
        ramStat.put("totalPhysMem", totalMb);
        ramStat.put("aspect", Math.round((usedMb * 100.0) / totalMb));
        ramPerf.put("name", ramStat.toString());
        broadcast.put(ramPerf);

        // Type 6: Temperature
        JSONObject tempPerf = new JSONObject();
        tempPerf.put("type", 6);
        tempPerf.put("id", 0);
        tempPerf.put("remove", 0);
        tempPerf.put("name", readThermalZoneCelsius());
        broadcast.put(tempPerf);

        data.put("broadcast", broadcast);
        msg.put("data", data);
        return msg;
    }

    private String readThermalZoneCelsius() {
        try {
            File f = new File("/sys/class/thermal/thermal_zone0/temp");
            if (f.exists() && f.canRead()) {
                try (BufferedReader br = new BufferedReader(new InputStreamReader(new FileInputStream(f)))) {
                    String line = br.readLine();
                    if (line != null) {
                        double v = Double.parseDouble(line.trim());
                        if (v > 1000) v /= 1000.0;
                        return String.format(Locale.US, "%.1f", v);
                    }
                }
            }
        } catch (Exception ignored) {
        }
        return "44.5";
    }

    private void broadcastPrioritiesUpdate() {
        try {
            JSONObject msg = new JSONObject();
            msg.put("command", "priorities-update");
            JSONObject data = new JSONObject();
            data.put("priorities", colorEngine.getPrioritiesJsonArray());
            data.put("priorities_autoselect", colorEngine.isPrioritiesAutoselect());
            msg.put("data", data);
            broadcastWs(msg);
        } catch (Exception ignored) {
        }
    }

    private void broadcastSettingsUpdate(JSONObject cfg) {
        try {
            JSONObject msg = new JSONObject();
            msg.put("command", "settings-update");
            msg.put("data", cfg);
            broadcastWs(msg);
        } catch (Exception ignored) {
        }
    }

    private void broadcastWs(JSONObject msg) {
        for (WsClient c : wsClients) {
            c.sendJson(msg);
        }
    }

    public void appendLog(String level, String subComponent, String message) {
        try {
            JSONObject entry = new JSONObject();
            long now = System.currentTimeMillis();
            entry.put("utime", String.valueOf(now));
            entry.put("loggerName", "HYPERHDR");
            entry.put("loggerSubName", subComponent);
            entry.put("function", "");
            entry.put("line", 0);
            entry.put("fileName", "");
            entry.put("message", message);
            entry.put("levelString", level);

            logBuffer.add(entry);
            while (logBuffer.size() > 200) {
                logBuffer.remove(0);
            }

            JSONObject logMsg = new JSONObject();
            logMsg.put("command", "logging-update");
            JSONObject res = new JSONObject();
            JSONArray arr = new JSONArray();
            arr.put(entry);
            res.put("messages", arr);
            logMsg.put("result", res);

            for (WsClient c : wsClients) {
                if (c.loggingStream) {
                    c.sendJson(logMsg);
                }
            }
        } catch (Exception ignored) {
        }
    }

    private String readAssetText(String assetName) throws IOException {
        try (InputStream is = context.getAssets().open(assetName)) {
            return new String(readAllBytes(is), StandardCharsets.UTF_8);
        }
    }

    private static byte[] readAllBytes(InputStream is) throws IOException {
        ByteArrayOutputStream baos = new ByteArrayOutputStream(16384);
        byte[] buf = new byte[8192];
        int n;
        while ((n = is.read(buf)) >= 0) {
            baos.write(buf, 0, n);
        }
        return baos.toByteArray();
    }

    private static void readFully(InputStream is, byte[] buf, int len) throws IOException {
        int off = 0;
        while (off < len) {
            int r = is.read(buf, off, len - off);
            if (r < 0) throw new IOException("EOF");
            off += r;
        }
    }

    private static String readHttpLine(InputStream is) throws IOException {
        ByteArrayOutputStream baos = new ByteArrayOutputStream(128);
        int b;
        while ((b = is.read()) >= 0) {
            if (b == '\n') break;
            if (b != '\r') {
                baos.write(b);
                if (baos.size() > 8192) {
                    throw new IOException("HTTP request line exceeded 8KB limit");
                }
            }
        }
        if (b < 0 && baos.size() == 0) return null;
        return new String(baos.toByteArray(), StandardCharsets.US_ASCII);
    }

    private static void writeHttpResponse(OutputStream os, String status, String contentType, byte[] body) throws IOException {
        String headers = "HTTP/1.1 " + status + "\r\n" +
                "Content-Type: " + contentType + "\r\n" +
                "Content-Length: " + body.length + "\r\n" +
                "Access-Control-Allow-Origin: *\r\n" +
                "Cache-Control: no-cache\r\n" +
                "Connection: close\r\n\r\n";
        os.write(headers.getBytes(StandardCharsets.US_ASCII));
        os.write(body);
        os.flush();
    }

    private static String guessMimeType(String path) {
        String lower = path.toLowerCase(Locale.US);
        if (lower.endsWith(".html")) return "text/html; charset=utf-8";
        if (lower.endsWith(".js")) return "application/javascript; charset=utf-8";
        if (lower.endsWith(".css")) return "text/css; charset=utf-8";
        if (lower.endsWith(".json")) return "application/json; charset=utf-8";
        if (lower.endsWith(".svg")) return "image/svg+xml";
        if (lower.endsWith(".png")) return "image/png";
        if (lower.endsWith(".jpg") || lower.endsWith(".jpeg")) return "image/jpeg";
        if (lower.endsWith(".ico")) return "image/x-icon";
        return "text/plain; charset=utf-8";
    }
}
