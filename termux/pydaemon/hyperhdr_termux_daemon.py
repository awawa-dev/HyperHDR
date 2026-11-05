#!/usr/bin/env python3
"""
HyperHDR v24.0.0-beta.0 Headless Daemon for Termux (Android)
Serves the official HyperHDR v24.0.0-beta.0 Web UI (:8090), WebSocket JSON-RPC & Binary JPEG Stream (:8090),
Raw TCP JSON-RPC (:19444), FlatBuffers Server (:19400), Protobuf Server (:19445),
Infinite Color Engine, real-time Android Screen Grabber (screencap / X11 / Framebuffer / ADB),
and WLED UDP Realtime (21324 DRGB/DNRGB + 4048 DDP) & REST driver.
"""
import argparse
import base64
import copy
import hashlib
import io
import json
import math
import mimetypes
import os
import platform
import shutil
import socket
import struct
import subprocess
import sys
import threading
import time
import urllib.request
import uuid
import zlib
from concurrent.futures import ThreadPoolExecutor

VERSION = "24.0.0-beta.0"
BUILD_STR = "24.0.0-beta.0 (Termux Headless)"
BUILD_TIME = "Oct 22 2026 22:30:57"


def get_lan_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"


def encode_rgb_to_png(rgb_bytes, width, height):
    """Pure-Python fast PNG encoder (zero external library dependencies) for WebSocket live video preview."""
    stride = width * 3
    raw_rows = bytearray((stride + 1) * height)
    for y in range(height):
        row_start = y * (stride + 1)
        raw_rows[row_start] = 0  # Filter type 0 (None)
        src_start = y * stride
        raw_rows[row_start + 1 : row_start + 1 + stride] = rgb_bytes[src_start : src_start + stride]

    def make_chunk(chunk_type, data):
        crc = zlib.crc32(chunk_type + data) & 0xFFFFFFFF
        return struct.pack("!I", len(data)) + chunk_type + data + struct.pack("!I", crc)

    ihdr = struct.pack("!IIBBBBB", width, height, 8, 2, 0, 0, 0)  # 8-bit RGB
    compressed = zlib.compress(bytes(raw_rows), level=3)
    return (
        b"\x89PNG\r\n\x1a\n"
        + make_chunk(b"IHDR", ihdr)
        + make_chunk(b"IDAT", compressed)
        + make_chunk(b"IEND", b"")
    )


class TermuxScreenGrabber:
    """
    Multi-backend real screen grabber for Termux & Android TV:
      1. Native Android `screencap` (direct or via `su -c screencap` / `/system/bin/screencap`)
      2. ADB loopback (`adb -s 127.0.0.1:5555 exec-out screencap`)
      3. Termux:X11 / Xvfb (`ffmpeg` / `import` / XWD if DISPLAY is set)
      4. Linux Framebuffer (`/dev/fb0` or `/dev/graphics/fb0`)
      5. External FlatBuffers (:19400), Protobuf (:19445), or JSON-RPC `image` frames
    Downscales captured frames to 192x108 RGB24 for low-latency LED zone mapping and WebSocket live video preview.
    """
    def __init__(self, target_width=256, target_height=144):
        self.target_width = target_width
        self.target_height = target_height
        self.target_fps = 30
        self.crop_left = 0
        self.crop_right = 0
        self.crop_top = 0
        self.crop_bottom = 0
        self.device_mode = "Android MediaProjection API"
        self.enabled = True
        self.running = True

        self.latest_rgb = None
        self.latest_w = 0
        self.latest_h = 0
        self.latest_frame_ts = 0.0
        self.last_mp_udp_ts = 0.0
        self.last_mp_prompt_ts = 0.0
        self.latest_preview_bytes = None
        self.last_preview_encoded_ts = 0.0
        self.active_backend = "Android MediaProjection API"
        self.current_fps = 0.0
        self.last_latency_ms = 2
        self._frames_in_window = 0
        self._window_start = time.time()
        self._lock = threading.Lock()
        self._preview_callbacks = []

    def add_preview_listener(self, cb):
        self._preview_callbacks.append(cb)

    def configure(self, sys_grab_cfg):
        if not isinstance(sys_grab_cfg, dict):
            return
        self.target_fps = max(5, min(60, int(sys_grab_cfg.get("fps", 30))))
        self.crop_left = max(0, int(sys_grab_cfg.get("cropLeft", 0)))
        self.crop_right = max(0, int(sys_grab_cfg.get("cropRight", 0)))
        self.crop_top = max(0, int(sys_grab_cfg.get("cropTop", 0)))
        self.crop_bottom = max(0, int(sys_grab_cfg.get("cropBottom", 0)))
        self.device_mode = str(sys_grab_cfg.get("device", "Android MediaProjection API"))

    def is_capturing(self):
        return (time.time() - self.latest_frame_ts) < 3.5

    def start(self):
        threading.Thread(target=self._mediaprojection_udp_listener, daemon=True).start()
        threading.Thread(target=self._capture_loop, daemon=True).start()
        threading.Thread(target=self._auto_prompt_mediaprojection_once, daemon=True).start()

    def _auto_prompt_mediaprojection_once(self):
        time.sleep(1.2)
        if not self.is_capturing():
            self.request_mediaprojection_permission()

    def request_mediaprojection_permission(self):
        now = time.time()
        if (now - self.last_mp_prompt_ts) < 2.0:
            return False
        self.last_mp_prompt_ts = now
        env = os.environ.copy()
        env.pop("LD_PRELOAD", None)
        if "/system/bin" not in env.get("PATH", ""):
            env["PATH"] = env.get("PATH", "") + ":/system/bin:/system/xbin"
        cmds = [
            [
                "/system/bin/am", "start", "--user", "0",
                "-n", "org.hyperhdr.androidtv/.MainActivity",
                "-a", "org.hyperhdr.androidtv.ACTION_REQUEST_MEDIA_PROJECTION",
                "--ez", "headless_bridge", "true",
            ],
            [
                "am", "start",
                "-n", "org.hyperhdr.androidtv/.MainActivity",
                "-a", "org.hyperhdr.androidtv.ACTION_REQUEST_MEDIA_PROJECTION",
                "--ez", "headless_bridge", "true",
            ],
        ]
        for cmd in cmds:
            try:
                p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
                out, err = p.communicate(timeout=2.5)
                if p.returncode == 0 and b"Error:" not in out and b"Error:" not in err:
                    self.active_backend = "Android MediaProjection API"
                    return True
            except Exception:
                pass
        return False

    def _mediaprojection_udp_listener(self):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.bind(("127.0.0.1", 19446))
            while self.running:
                data, _ = sock.recvfrom(65535)
                if not self.enabled or len(data) < 12 or data[:4] != b"HHMP":
                    continue
                t0 = time.time()
                bw, bh = struct.unpack("!HH", data[4:8])
                rgb = data[8:]
                if bw > 0 and bh > 0 and len(rgb) >= bw * bh * 3:
                    self.last_mp_udp_ts = t0
                    self._on_new_frame(rgb[: bw * bh * 3], bw, bh, "Android MediaProjection API", t0)
        except Exception:
            pass

    def _try_mediaprojection_http(self):
        for port in (8091, 8090):
            try:
                t0 = time.time()
                req = urllib.request.Request(f"http://127.0.0.1:{port}/mediaprojection-frame.rgb")
                with urllib.request.urlopen(req, timeout=0.35) as resp:
                    if resp.status == 200:
                        data = resp.read()
                        if len(data) >= 12 and data[:4] == b"HHMP":
                            bw, bh = struct.unpack("!HH", data[4:8])
                            rgb = data[8:]
                            if bw > 0 and bh > 0 and len(rgb) >= bw * bh * 3:
                                self._on_new_frame(rgb[: bw * bh * 3], bw, bh, "Android MediaProjection API", t0)
                                return True
            except Exception:
                pass
        return False

    def inject_external_frame(self, rgb_bytes, width, height, source_name="External Stream"):
        if not rgb_bytes or width <= 0 or height <= 0:
            return
        t0 = time.time()
        if width != self.target_width or height != self.target_height:
            rgb_bytes = self._Downscale_rgb24(rgb_bytes, width, height, self.target_width, self.target_height)
            width, height = self.target_width, self.target_height
        self._on_new_frame(rgb_bytes, width, height, source_name, t0)

    def _capture_loop(self):
        while self.running:
            t0 = time.time()
            if not self.enabled:
                time.sleep(0.25)
                continue

            # If MediaProjection UDP bridge or external FlatBuffers/Protobuf/JSON-RPC stream is pushing frames, yield to it
            if (t0 - self.last_mp_udp_ts) < 1.0:
                time.sleep(0.08)
                continue
            if self.active_backend in ("FlatBuffers", "Protobuf", "JSON-RPC Image") and (t0 - self.latest_frame_ts) < 1.5:
                time.sleep(0.1)
                continue

            captured = (
                self._try_mediaprojection_http()
                or self._try_screencap_cmd(["/system/bin/screencap"], "Android MediaProjection / screencap")
                or self._try_screencap_cmd(["screencap"], "Android MediaProjection / screencap")
                or self._try_screencap_cmd(["su", "-c", "/system/bin/screencap"], "Android Root screencap")
                or self._try_adb_loopback()
                or self._try_framebuffer()
                or self._try_x11_grab()
            )

            elapsed = time.time() - t0
            min_interval = 1.0 / max(5, self.target_fps)
            if captured:
                sleep_s = max(0.01, min_interval - elapsed)
            else:
                sleep_s = 0.5
            time.sleep(sleep_s)

    def _try_screencap_cmd(self, cmd, label):
        try:
            t0 = time.time()
            env = os.environ.copy()
            # Ensure Android system libraries are accessible when invoking /system/bin/screencap from Termux
            env.pop("LD_PRELOAD", None)
            if "/system/lib64" not in env.get("LD_LIBRARY_PATH", ""):
                ld = env.get("LD_LIBRARY_PATH", "")
                env["LD_LIBRARY_PATH"] = f"/system/lib64:/system/lib:{ld}" if ld else "/system/lib64:/system/lib"
            proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, env=env)
            raw = proc.communicate(timeout=1.8)[0]
            if proc.returncode != 0 or not raw or len(raw) < 64:
                return False
            parsed = self._parse_raw_screencap(raw)
            if not parsed:
                return False
            rgb_bytes, w, h = parsed
            self._on_new_frame(rgb_bytes, w, h, label, t0)
            return True
        except Exception:
            return False

    def _try_adb_loopback(self):
        if not shutil.which("adb"):
            return False
        try:
            t0 = time.time()
            proc = subprocess.Popen(
                ["adb", "exec-out", "screencap"],
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
            )
            raw = proc.communicate(timeout=1.8)[0]
            if proc.returncode != 0 or not raw or len(raw) < 64:
                return False
            parsed = self._parse_raw_screencap(raw)
            if not parsed:
                return False
            rgb_bytes, w, h = parsed
            self._on_new_frame(rgb_bytes, w, h, "ADB Loopback screencap", t0)
            return True
        except Exception:
            return False

    def _try_framebuffer(self):
        for fb_path in ("/dev/graphics/fb0", "/dev/fb0"):
            if os.path.exists(fb_path) and os.access(fb_path, os.R_OK):
                try:
                    t0 = time.time()
                    with open(fb_path, "rb") as f:
                        raw = f.read(1920 * 1080 * 4)
                    if len(raw) >= 640 * 480 * 4:
                        # Infer common resolution from buffer length
                        pixels = len(raw) // 4
                        w, h = 1920, 1080
                        if pixels == 1280 * 720:
                            w, h = 1280, 720
                        elif pixels < 1920 * 1080:
                            w = int(math.sqrt(pixels * 16 / 9))
                            h = max(1, pixels // max(1, w))
                        rgb = self._downscale_rgba_to_rgb24(raw, w, h, self.target_width, self.target_height)
                        self._on_new_frame(rgb, self.target_width, self.target_height, f"Framebuffer ({fb_path})", t0)
                        return True
                except Exception:
                    pass
        return False

    def _try_x11_grab(self):
        disp = os.environ.get("DISPLAY")
        if not disp or not shutil.which("ffmpeg"):
            return False
        try:
            t0 = time.time()
            w, h = self.target_width, self.target_height
            proc = subprocess.Popen(
                [
                    "ffmpeg", "-loglevel", "quiet",
                    "-f", "x11grab", "-i", disp,
                    "-vframes", "1", "-vf", f"scale={w}:{h}",
                    "-f", "rawvideo", "-pix_fmt", "rgb24", "-",
                ],
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
            )
            raw = proc.communicate(timeout=1.5)[0]
            if len(raw) == w * h * 3:
                self._on_new_frame(raw, w, h, f"Termux:X11 ({disp})", t0)
                return True
        except Exception:
            pass
        return False

    def _parse_raw_screencap(self, raw):
        if len(raw) < 16:
            return None
        w, h, fmt = struct.unpack_from("<III", raw, 0)
        if w <= 0 or h <= 0 or w > 7680 or h > 4320:
            return None
        expected_rgba = w * h * 4
        if len(raw) >= 16 + expected_rgba:
            hdr_size = 16  # Android 11+ (w, h, format, dataspace)
        elif len(raw) >= 12 + expected_rgba:
            hdr_size = 12  # Android 9/10 (w, h, format)
        else:
            return None
        rgb = self._downscale_rgba_to_rgb24(
            memoryview(raw)[hdr_size : hdr_size + expected_rgba],
            w, h, self.target_width, self.target_height,
        )
        return rgb, self.target_width, self.target_height

    def _downscale_rgba_to_rgb24(self, rgba_buf, src_w, src_h, dst_w, dst_h):
        c_l = min(src_w // 3, self.crop_left)
        c_r = min(src_w // 3, self.crop_right)
        c_t = min(src_h // 3, self.crop_top)
        c_b = min(src_h // 3, self.crop_bottom)
        act_w = max(16, src_w - c_l - c_r)
        act_h = max(16, src_h - c_t - c_b)

        out = bytearray(dst_w * dst_h * 3)
        src_stride = src_w * 4
        x_offsets = [((c_l + (dx * act_w) // dst_w) * 4) for dx in range(dst_w)]

        out_idx = 0
        for dy in range(dst_h):
            sy = c_t + (dy * act_h) // dst_h
            row_base = sy * src_stride
            for x_off in x_offsets:
                p = row_base + x_off
                out[out_idx] = rgba_buf[p]
                out[out_idx + 1] = rgba_buf[p + 1]
                out[out_idx + 2] = rgba_buf[p + 2]
                out_idx += 3
        return bytes(out)

    @staticmethod
    def _Downscale_rgb24(rgb_buf, src_w, src_h, dst_w, dst_h):
        out = bytearray(dst_w * dst_h * 3)
        src_stride = src_w * 3
        x_offsets = [((dx * src_w) // dst_w) * 3 for dx in range(dst_w)]
        out_idx = 0
        for dy in range(dst_h):
            sy = (dy * src_h) // dst_h
            row_base = sy * src_stride
            for x_off in x_offsets:
                p = row_base + x_off
                out[out_idx] = rgb_buf[p]
                out[out_idx + 1] = rgb_buf[p + 1]
                out[out_idx + 2] = rgb_buf[p + 2]
                out_idx += 3
        return bytes(out)

    def _on_new_frame(self, rgb_bytes, w, h, backend_name, t0):
        now = time.time()
        with self._lock:
            self.latest_rgb = rgb_bytes
            self.latest_w = w
            self.latest_h = h
            self.latest_frame_ts = now
            self.active_backend = backend_name
            self.last_latency_ms = max(1, int((now - t0) * 1000))
            self._frames_in_window += 1
            dt = now - self._window_start
            if dt >= 1.0:
                self.current_fps = self._frames_in_window / dt
                self._frames_in_window = 0
                self._window_start = now

        if (now - self.last_preview_encoded_ts) >= 0.12 or self.latest_preview_bytes is None:
            self.last_preview_encoded_ts = now
            try:
                png = encode_rgb_to_png(rgb_bytes, w, h)
                self.latest_preview_bytes = png
                for cb in self._preview_callbacks:
                    cb(png)
            except Exception:
                pass

    def generate_fallback_preview_png(self, current_leds=None):
        if self.latest_preview_bytes and self.is_capturing():
            return self.latest_preview_bytes
        w, h = 160, 90
        buf = bytearray(w * h * 3)
        for y in range(h):
            r_bg = 18 + (y * 16) // h
            g_bg = 26 + (y * 24) // h
            b_bg = 42 + (y * 32) // h
            row_off = y * w * 3
            for x in range(w):
                p = row_off + x * 3
                buf[p] = r_bg
                buf[p + 1] = g_bg
                buf[p + 2] = b_bg

        if current_leds and len(current_leds) >= 12:
            led_cnt = len(current_leds) // 3
            for x in range(w):
                top_idx = (x * (led_cnt // 3)) // w
                bot_idx = min(led_cnt - 1, (led_cnt // 2) + ((w - 1 - x) * (led_cnt // 3)) // w)
                tr, tg, tb = current_leds[top_idx * 3 : top_idx * 3 + 3]
                br, bg, bb = current_leds[bot_idx * 3 : bot_idx * 3 + 3]
                for by in range(6):
                    tp = (by * w + x) * 3
                    bp = ((h - 1 - by) * w + x) * 3
                    buf[tp : tp + 3] = bytes((tr, tg, tb))
                    buf[bp : bp + 3] = bytes((br, bg, bb))

        return encode_rgb_to_png(bytes(buf), w, h)


class WledDriver:
    def __init__(self):
        self.device_type = "wled"
        self.host = "192.168.1.150"
        self.port = 21324
        self.color_order = "rgb"
        self.invert = False
        self.brightness_max = True
        self.brightness_max_level = 255
        self.restore_original_state = False
        self.enabled = True
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.packets_sent = 0
        self.packets_window = 0
        self.packets_dropped = 0
        self.current_fps = 0.0
        self.last_window_t = time.time()

    def configure(self, dev_cfg):
        if not isinstance(dev_cfg, dict):
            return
        self.device_type = str(dev_cfg.get("type", "wled")).lower()
        raw_host = str(dev_cfg.get("host", dev_cfg.get("output", self.host))).strip()
        if ":" in raw_host:
            h, p = raw_host.split(":", 1)
            self.host = h
            try:
                self.port = int(p)
            except ValueError:
                pass
        else:
            if raw_host:
                self.host = raw_host
            self.port = int(dev_cfg.get("port", 4048 if self.device_type in ("ddp", "hyperk") else 21324))
        self.color_order = str(dev_cfg.get("colorOrder", "rgb")).lower()
        self.invert = bool(dev_cfg.get("invert", False))
        self.brightness_max = bool(dev_cfg.get("brightnessMax", True))
        self.brightness_max_level = int(dev_cfg.get("brightnessMaxLevel", 255))
        self.restore_original_state = bool(dev_cfg.get("restoreOriginalState", False))
        if self.enabled and self.device_type in ("wled", "hyperk") and self.host:
            threading.Thread(target=self._prep_wled, daemon=True).start()

    def _prep_wled(self):
        try:
            body = {"on": True}
            if self.brightness_max:
                body["bri"] = max(1, min(255, self.brightness_max_level))
            req = urllib.request.Request(
                f"http://{self.host}/json/state",
                data=json.dumps(body).encode("utf-8"),
                headers={"Content-Type": "application/json"},
                method="POST",
            )
            urllib.request.urlopen(req, timeout=0.7).read()
        except Exception:
            pass

    def send_leds(self, rgb_flat):
        if not self.enabled or not rgb_flat or not self.host:
            return
        if self.device_type == "file":
            self._record(True)
            return
        try:
            led_count = len(rgb_flat) // 3
            ordered = bytearray(led_count * 3)
            co = self.color_order
            inv = self.invert
            for i in range(led_count):
                r = max(0, min(255, int(rgb_flat[i * 3])))
                g = max(0, min(255, int(rgb_flat[i * 3 + 1])))
                b = max(0, min(255, int(rgb_flat[i * 3 + 2])))
                if inv:
                    r, g, b = 255 - r, 255 - g, 255 - b
                if co == "bgr":
                    b0, b1, b2 = b, g, r
                elif co == "grb":
                    b0, b1, b2 = g, r, b
                elif co == "brg":
                    b0, b1, b2 = b, r, g
                elif co == "gbr":
                    b0, b1, b2 = g, b, r
                elif co == "rbg":
                    b0, b1, b2 = r, b, g
                else:
                    b0, b1, b2 = r, g, b
                ordered[i * 3] = b0
                ordered[i * 3 + 1] = b1
                ordered[i * 3 + 2] = b2

            if self.device_type in ("ddp", "hyperk"):
                target_port = self.port if self.port != 21324 else 4048
                max_chunk = 480 * 3
                offset = 0
                seq = 1
                total = len(ordered)
                while offset < total:
                    chunk = ordered[offset : offset + max_chunk]
                    is_last = (offset + len(chunk)) >= total
                    hdr = (
                        bytes([0x41 if is_last else 0x40, seq & 0x0F, 0x01, 0x01])
                        + struct.pack("!IH", offset, len(chunk))
                    )
                    self.sock.sendto(hdr + chunk, (self.host, target_port))
                    offset += len(chunk)
                    seq += 1
            elif self.device_type == "udpraw":
                self.sock.sendto(ordered, (self.host, self.port))
            else:
                target_port = self.port if self.port > 0 else 21324
                if led_count <= 490:
                    pkt = bytes([2, 2]) + ordered
                    self.sock.sendto(pkt, (self.host, target_port))
                else:
                    chunk_leds = 489
                    for start in range(0, led_count, chunk_leds):
                        cnt = min(chunk_leds, led_count - start)
                        hdr = struct.pack("!BBH", 4, 2, start)
                        self.sock.sendto(hdr + ordered[start * 3 : (start + cnt) * 3], (self.host, target_port))
            self._record(True)
        except Exception:
            self._record(False)

    def _record(self, ok):
        if ok:
            self.packets_sent += 1
            self.packets_window += 1
        else:
            self.packets_dropped += 1
        now = time.time()
        dt = now - self.last_window_t
        if dt >= 1.0:
            self.current_fps = self.packets_window / dt
            self.packets_window = 0
            self.last_window_t = now

    def discover_devices(self):
        found = []
        lan_ip = get_lan_ip()
        candidates = []
        if self.host and self.host != "127.0.0.1":
            candidates.append(self.host)
        if "." in lan_ip and not lan_ip.startswith("127."):
            prefix = lan_ip.rsplit(".", 1)[0] + "."
            for i in range(1, 255):
                ip = prefix + str(i)
                if ip not in candidates:
                    candidates.append(ip)

        def probe(ip):
            try:
                with urllib.request.urlopen(f"http://{ip}/json/info", timeout=0.22) as r:
                    if r.status == 200:
                        data = json.loads(r.read().decode("utf-8"))
                        if "leds" in data:
                            name = data.get("name", "WLED")
                            ver = data.get("ver", "")
                            cnt = data.get("leds", {}).get("count", 0)
                            return {
                                "name": f"{name} ({ip}, {cnt} LEDs, v{ver})",
                                "value": ip,
                                "ip": ip,
                                "host": name,
                                "port": 80,
                                "ledCount": cnt,
                            }
            except Exception:
                return None

        with ThreadPoolExecutor(max_workers=32) as ex:
            for res in ex.map(probe, candidates):
                if res:
                    found.append(res)
        if not found and self.host:
            found.append({"name": f"{self.host} (Configured WLED)", "value": self.host, "ip": self.host, "port": 80})
        return found

    def identify(self, target_host=None):
        h = target_host or self.host
        def _blink():
            try:
                s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                for c in ((255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255)):
                    pkt = bytes([2, 2]) + bytes(c) * 120
                    s.sendto(pkt, (h, 21324))
                    time.sleep(0.35)
                s.close()
            except Exception:
                pass
        threading.Thread(target=_blink, daemon=True).start()


class HeadlessHyperHdrDaemon:
    def __init__(self, assets_dir, userdata_dir, web_port=8090, json_port=19444, wled_host=None):
        self.assets_dir = assets_dir
        self.www_dir = os.path.join(assets_dir, "www")
        self.userdata_dir = userdata_dir
        os.makedirs(self.userdata_dir, exist_ok=True)
        self.db_path = os.path.join(self.userdata_dir, "hyperhdr_db.json")

        with open(os.path.join(assets_dir, "server_schema.json"), "r", encoding="utf-8") as f:
            self.server_schema = json.load(f)
        with open(os.path.join(assets_dir, "default_config.json"), "r", encoding="utf-8") as f:
            self.default_config = json.load(f)

        self.wled = WledDriver()
        self.grabber = TermuxScreenGrabber(192, 108)
        self.grabber.add_preview_listener(self._on_grabber_preview_frame)

        self.current_instance = 0
        self.ws_clients = []
        self.active_session_tokens = set()
        self.ws_lock = threading.Lock()
        self.logs = []
        self.running = True
        self.phase = 0.0
        self.smoothing_fps = 30.0
        self.last_rgb_flat = []
        self.current_float_rgb = []

        self.components = {
            "ALL": True, "SMOOTHING": True, "BLACKBORDER": True,
            "FORWARDER": False, "BOBLIGHTSERVER": False, "GRABBER": True,
            "SYSTEMGRABBER": True, "VIDEOGRABBER": True, "FLATBUFSERVER": True,
            "PROTOSERVER": True, "RAWUDPSERVER": False, "LEDDEVICE": True, "HDR": False,
        }
        self.priorities = {
            240: {
                "priority": 240, "componentId": "VIDEOGRABBER",
                "origin": "System", "owner": "Android MediaProjection API",
                "active": True, "visible": False, "duration_ms": -1,
            },
            250: {
                "priority": 250, "componentId": "SYSTEMGRABBER",
                "origin": "System", "owner": "Android MediaProjection API",
                "active": True, "visible": True, "duration_ms": -1,
            },
        }
        self.priorities_autoselect = True
        self.manual_priority = 250
        self.mapping_type = "multicolor_mean"
        self.videomode = "2D"
        self.videomodehdr = 0

        # Black border detector state
        self.bb_enabled = True
        self.bb_threshold = 5  # percent
        self.bb_blur_remove = 1
        self.bb_border_switch_cnt = 10
        self.bb_unknown_timeout = 300
        self.detected_crop_top = 0
        self.detected_crop_bottom = 0
        self.detected_crop_left = 0
        self.detected_crop_right = 0
        self.candidate_crop_y = 0
        self.candidate_crop_x = 0
        self.consistent_frames_y = 0
        self.consistent_frames_x = 0
        self.unknown_frames_y = 0
        self.unknown_frames_x = 0

        self._load_db(wled_host, web_port, json_port)

    def _load_db(self, wled_host, web_port, json_port):
        tmp_path = self.db_path + ".tmp"
        if (not os.path.exists(self.db_path) or os.path.getsize(self.db_path) == 0) and os.path.exists(tmp_path) and os.path.getsize(tmp_path) > 0:
            try:
                os.replace(tmp_path, self.db_path)
            except Exception:
                pass

        if os.path.exists(self.db_path) and os.path.getsize(self.db_path) > 0:
            try:
                with open(self.db_path, "r", encoding="utf-8") as f:
                    self.db = json.load(f)
            except Exception:
                self.db = None
        else:
            self.db = None

        if not self.db:
            cfg0 = copy.deepcopy(self.default_config)
            self.db = {
                "version": "HyperHDR_export_format_v20",
                "adminPassword": "hyperhdr",
                "defaultPasswordSet": True,
                "tokens": [],
                "instances": [{"instance": 0, "friendly_name": "First LED instance", "running": True}],
                "settings": {"0": cfg0},
            }
        else:
            self.current_instance = self.db.get("currentInstance", 0)

        cfg = self.get_active_config()
        cfg.setdefault("systemControl", {}).setdefault("systemInstanceEnable", True)
        cfg.setdefault("videoControl", {}).setdefault("videoInstanceEnable", True)
        bb_obj = cfg.setdefault("blackborderdetector", {})
        if not bb_obj.get("enable", False):
            bb_obj["enable"] = True
        sys_grab = cfg.setdefault("systemGrabber", {})
        if sys_grab.get("device", "auto") in ("", "auto") or "Pipewire" in str(sys_grab.get("device", "")) or "screencap" in str(sys_grab.get("device", "")):
            sys_grab["device"] = "Android MediaProjection API"
        vid_grab = cfg.setdefault("videoGrabber", {})
        if vid_grab.get("device", "auto") in ("", "auto", "Termux-ScreenGrabber-0", "Android-MediaProjection-0"):
            vid_grab["device"] = "Android MediaProjection API"
        if wled_host:
            cfg.setdefault("device", {})["host"] = wled_host
            cfg["device"]["type"] = "wled"
        if web_port:
            cfg.setdefault("webConfig", {})["port"] = web_port
        if json_port:
            cfg.setdefault("jsonServer", {})["port"] = json_port
        self.http_port = int(cfg.get("webConfig", {}).get("port", 8090))
        self.json_port = int(cfg.get("jsonServer", {}).get("port", 19444))
        self.flatbuf_port = int(cfg.get("flatbufServer", {}).get("port", 19400))
        self.proto_port = int(cfg.get("protoServer", {}).get("port", 19445))
        self._save_db()
        self.wled.configure(cfg.get("device", {}))
        self.grabber.configure(cfg.get("systemGrabber", {}))
        self.log("INFO", "DAEMON", f"HyperHDR v{VERSION} initialized (userdata: {self.userdata_dir})")

    def _save_db(self):
        try:
            tmp_path = self.db_path + ".tmp"
            with open(tmp_path, "w", encoding="utf-8") as f:
                json.dump(self.db, f, indent=2)
                f.flush()
                os.fsync(f.fileno())
            os.replace(tmp_path, self.db_path)
        except Exception:
            pass

    def get_active_config(self):
        s = self.db.setdefault("settings", {})
        k = str(self.current_instance)
        if k not in s:
            s[k] = copy.deepcopy(self.default_config)
        return s[k]

    def log(self, level, sub, msg):
        ts = time.strftime("%Y-%m-%d %H:%M:%S")
        print(f"[{ts}] [{level}] [{sub}] {msg}", flush=True)
        entry = {
            "utime": str(int(time.time() * 1000)),
            "loggerName": "HYPERHDR",
            "loggerSubName": sub,
            "function": "",
            "line": 0,
            "fileName": "",
            "message": msg,
            "levelString": level,
        }
        self.logs.append(entry)
        if len(self.logs) > 200:
            self.logs.pop(0)
        self.broadcast_ws({"command": "logging-update", "result": {"messages": [entry]}}, only_attr="logging_stream")

    def start(self):
        self.grabber.start()
        threading.Thread(target=self._engine_loop, daemon=True).start()
        threading.Thread(target=self._telemetry_loop, daemon=True).start()
        threading.Thread(target=self._json_rpc_server, daemon=True).start()
        threading.Thread(target=self._flatbuf_server, daemon=True).start()
        threading.Thread(target=self._proto_server, daemon=True).start()
        self._http_ws_server()

    def _on_grabber_preview_frame(self, png_bytes):
        with self.ws_lock:
            clients = [c for c in self.ws_clients if c.get("image_stream")]
        for c in clients:
            try:
                self._ws_send(c, 0x2, png_bytes)
            except Exception:
                pass

    def _detect_black_borders(self, rgb_bytes, width, height):
        if not rgb_bytes or width <= 0 or height <= 0:
            return

        cfg = self.get_active_config()
        bb_cfg = cfg.get("blackborderdetector", {})
        thresh_pct = max(1, min(50, int(bb_cfg.get("threshold", self.bb_threshold))))
        thresh = max(1, int(thresh_pct * 255 / 100))
        max_h = height // 3
        max_w = width // 3
        stride = width * 3

        x_mid = width // 2
        x33 = width // 3
        x66 = (width * 2) // 3
        x25 = width // 4
        x75 = (width * 3) // 4

        y_mid = height // 2
        y33 = height // 3
        y66 = (height * 2) // 3

        def is_bright(pos):
            return rgb_bytes[pos] > thresh or rgb_bytes[pos + 1] > thresh or rgb_bytes[pos + 2] > thresh

        # 1. Letterbox (top & bottom horizontal black bars)
        detected_y = -1
        for y in range(max_h):
            top_off = y * stride
            bot_off = (height - 1 - y) * stride
            if (
                is_bright(top_off + x_mid * 3)
                or is_bright(top_off + x33 * 3)
                or is_bright(top_off + x66 * 3)
                or is_bright(top_off + x25 * 3)
                or is_bright(top_off + x75 * 3)
                or is_bright(bot_off + x_mid * 3)
                or is_bright(bot_off + x33 * 3)
                or is_bright(bot_off + x66 * 3)
            ):
                detected_y = y
                break

        if detected_y == -1:
            self.unknown_frames_y += 1
            if self.unknown_frames_y >= self.bb_unknown_timeout:
                self.detected_crop_top = 0
                self.detected_crop_bottom = 0
                self.candidate_crop_y = 0
                self.consistent_frames_y = 0
        else:
            self.unknown_frames_y = 0
            y_border = min(max_h, detected_y + self.bb_blur_remove) if detected_y > 0 else 0
            if abs(y_border - self.candidate_crop_y) <= 2:
                self.consistent_frames_y += 1
                if self.consistent_frames_y >= self.bb_border_switch_cnt and abs(y_border - self.detected_crop_top) > 1:
                    self.detected_crop_top = y_border
                    self.detected_crop_bottom = y_border
            else:
                self.candidate_crop_y = y_border
                self.consistent_frames_y = 0

        # 2. Pillarbox (left & right vertical black bars)
        detected_x = -1
        for x in range(max_w):
            rx = width - 1 - x
            if (
                is_bright(y_mid * stride + x * 3)
                or is_bright(y33 * stride + x * 3)
                or is_bright(y66 * stride + x * 3)
                or is_bright(y_mid * stride + rx * 3)
                or is_bright(y33 * stride + rx * 3)
                or is_bright(y66 * stride + rx * 3)
            ):
                detected_x = x
                break

        if detected_x == -1:
            self.unknown_frames_x += 1
            if self.unknown_frames_x >= self.bb_unknown_timeout:
                self.detected_crop_left = 0
                self.detected_crop_right = 0
                self.candidate_crop_x = 0
                self.consistent_frames_x = 0
        else:
            self.unknown_frames_x = 0
            x_border = min(max_w, detected_x + self.bb_blur_remove) if detected_x > 0 else 0
            if abs(x_border - self.candidate_crop_x) <= 2:
                self.consistent_frames_x += 1
                if self.consistent_frames_x >= self.bb_border_switch_cnt and abs(x_border - self.detected_crop_left) > 1:
                    self.detected_crop_left = x_border
                    self.detected_crop_right = x_border
            else:
                self.candidate_crop_x = x_border
                self.consistent_frames_x = 0

    def _sample_rgb_frame_to_leds(self, rgb_bytes, width, height, leds):
        led_count = max(1, len(leds))
        out = [0.0] * (led_count * 3)
        if not rgb_bytes or width <= 0 or height <= 0:
            return out

        if self.components.get("BLACKBORDER", True):
            self._detect_black_borders(rgb_bytes, width, height)
            c_top = self.detected_crop_top
            c_bottom = max(c_top + 16, height - self.detected_crop_bottom)
            c_left = self.detected_crop_left
            c_right = max(c_left + 16, width - self.detected_crop_right)
        else:
            c_top = 0
            c_bottom = height
            c_left = 0
            c_right = width

        active_w = max(1, c_right - c_left)
        active_h = max(1, c_bottom - c_top)

        if self.mapping_type == "unicolor_mean":
            sr = sg = sb = cnt = 0
            stride = width * 3
            for y in range(c_top, c_bottom, 2):
                roff = y * stride
                for x in range(c_left, c_right, 2):
                    p = roff + x * 3
                    sr += rgb_bytes[p]
                    sg += rgb_bytes[p + 1]
                    sb += rgb_bytes[p + 2]
                    cnt += 1
            if cnt > 0:
                mr, mg, mb = sr / cnt, sg / cnt, sb / cnt
                for i in range(led_count):
                    out[i * 3] = mr
                    out[i * 3 + 1] = mg
                    out[i * 3 + 2] = mb
            return out

        stride = width * 3
        for i, z in enumerate(leds):
            if z.get("disabled", False):
                continue
            x0 = c_left + max(0, min(active_w - 1, int(float(z.get("hmin", 0.0)) * active_w)))
            x1 = c_left + max(x0 - c_left + 1, min(active_w, int(math.ceil(float(z.get("hmax", 1.0)) * active_w))))
            y0 = c_top + max(0, min(active_h - 1, int(float(z.get("vmin", 0.0)) * active_h)))
            y1 = c_top + max(y0 - c_top + 1, min(active_h, int(math.ceil(float(z.get("vmax", 1.0)) * active_h))))
            sr = sg = sb = cnt = 0
            for y in range(y0, y1):
                roff = y * stride
                for x in range(x0, x1):
                    p = roff + x * 3
                    sr += rgb_bytes[p]
                    sg += rgb_bytes[p + 1]
                    sb += rgb_bytes[p + 2]
                    cnt += 1
            if cnt > 0:
                out[i * 3] = sr / cnt
                out[i * 3 + 1] = sg / cnt
                out[i * 3 + 2] = sb / cnt
        return out

    def _engine_loop(self):
        last_t = time.time()
        last_stream_t = 0.0
        while self.running:
            now = time.time()
            dt = max(0.005, now - last_t)
            last_t = now
            self.phase += dt

            cfg = self.get_active_config()
            leds = cfg.get("leds", [])
            led_count = max(1, len(leds))
            hz = float(cfg.get("smoothing", {}).get("updateFrequency", 30.0))
            self.smoothing_fps = hz

            if len(self.current_float_rgb) != led_count * 3:
                self.current_float_rgb = [0.0] * (led_count * 3)

            if not self.components.get("ALL", True):
                rgb_flat = [0] * (led_count * 3)
            else:
                vis = self._get_visible_priority()
                target_rgb = [0.0] * (led_count * 3)
                if vis and vis.get("componentId") == "COLOR" and "value" in vis:
                    c = vis["value"]["RGB"]
                    for i in range(led_count):
                        target_rgb[i * 3 : i * 3 + 3] = [float(c[0]), float(c[1]), float(c[2])]
                elif vis and vis.get("componentId") == "IMAGE" and "image_rgb" in vis:
                    target_rgb = self._sample_rgb_frame_to_leds(
                        vis["image_rgb"], vis.get("image_w", 192), vis.get("image_h", 108), leds
                    )
                elif vis and vis.get("componentId") == "EFFECT":
                    eff_name = vis.get("owner", "Rainbow swirl").lower()
                    speed = 1.6 if "fast" in eff_name else 0.5
                    for i in range(led_count):
                        pos = (i / led_count + self.phase * speed * 0.25) % 1.0
                        r, g, b = self._hsv_to_rgb(pos, 1.0, 1.0)
                        target_rgb[i * 3] = float(r)
                        target_rgb[i * 3 + 1] = float(g)
                        target_rgb[i * 3 + 2] = float(b)
                elif (
                    self.components.get("GRABBER", True)
                    and self.components.get("SYSTEMGRABBER", True)
                    and self.grabber.latest_rgb is not None
                    and self.grabber.is_capturing()
                ):
                    target_rgb = self._sample_rgb_frame_to_leds(
                        self.grabber.latest_rgb, self.grabber.latest_w, self.grabber.latest_h, leds
                    )
                else:
                    speed = 0.5
                    for i in range(led_count):
                        pos = (i / led_count + self.phase * speed * 0.25) % 1.0
                        r, g, b = self._hsv_to_rgb(pos, 1.0, 1.0)
                        target_rgb[i * 3] = float(r)
                        target_rgb[i * 3 + 1] = float(g)
                        target_rgb[i * 3 + 2] = float(b)

                # Minimum backlight threshold preservation (prevents signal drop on black scenes/blackbars)
                ca_list = cfg.get("color", {}).get("channelAdjustment", [])
                ca = ca_list[0] if ca_list else {}
                try:
                    bt = float(ca.get("backlightThreshold", 0.0039))
                except (ValueError, TypeError):
                    bt = 0.0039
                colored_bl = bool(ca.get("backlightColored", True))
                bri_scale = max(0.0, min(100.0, float(ca.get("brightness", 100)))) / 100.0

                if bt > 0.0001:
                    norm = (bt / 100.0) if bt > 1.0 else bt
                    min_floor = max(1.0, norm * 255.0 * bri_scale)
                    for i in range(led_count):
                        r = target_rgb[i * 3]
                        g = target_rgb[i * 3 + 1]
                        b = target_rgb[i * 3 + 2]
                        if colored_bl:
                            target_rgb[i * 3] = max(r, min_floor)
                            target_rgb[i * 3 + 1] = max(g, min_floor)
                            target_rgb[i * 3 + 2] = max(b, min_floor)
                        else:
                            avg = (r + g + b) / 3.0
                            if avg < min_floor:
                                target_rgb[i * 3] = max(r, min_floor)
                                target_rgb[i * 3 + 1] = max(g, min_floor)
                                target_rgb[i * 3 + 2] = max(b, min_floor)

                # Smooth interpolation
                smooth_en = self.components.get("SMOOTHING", True)
                time_ms = max(15.0, float(cfg.get("smoothing", {}).get("time_ms", 150.0)))
                alpha = min(1.0, (dt * 1000.0) / time_ms) if smooth_en else 1.0
                rgb_flat = [0] * (led_count * 3)
                for idx in range(led_count * 3):
                    nxt = self.current_float_rgb[idx] + (target_rgb[idx] - self.current_float_rgb[idx]) * alpha
                    self.current_float_rgb[idx] = nxt
                    rgb_flat[idx] = max(0, min(255, int(round(nxt))))

                self.last_rgb_flat = rgb_flat
                if self.components.get("LEDDEVICE", True):
                    self.wled.send_leds(rgb_flat)

            if now - last_stream_t >= 0.06:
                last_stream_t = now
                self.broadcast_ws(
                    {"command": "ledcolors-ledstream-update", "success": True, "result": {"leds": rgb_flat}},
                    only_attr="led_stream",
                )

            time.sleep(max(0.01, 1.0 / max(5.0, min(100.0, hz))))

    @staticmethod
    def _hsv_to_rgb(h, s, v):
        i = int(h * 6.0)
        f = (h * 6.0) - i
        p = v * (1.0 - s)
        q = v * (1.0 - f * s)
        t = v * (1.0 - (1.0 - f) * s)
        i %= 6
        if i == 0: r, g, b = v, t, p
        elif i == 1: r, g, b = q, v, p
        elif i == 2: r, g, b = p, v, t
        elif i == 3: r, g, b = p, q, v
        elif i == 4: r, g, b = t, p, v
        else: r, g, b = v, p, q
        return int(r * 255), int(g * 255), int(b * 255)

    def _get_visible_priority(self):
        now = time.time()
        for k, v in list(self.priorities.items()):
            exp = v.get("expires_at")
            if exp and now >= exp:
                del self.priorities[k]

        if 250 in self.priorities:
            self.priorities[250]["active"] = bool(
                self.components.get("GRABBER", True) and self.components.get("SYSTEMGRABBER", True)
            )
            self.priorities[250]["owner"] = f"Android MediaProjection API ({self.grabber.active_backend})"

        items = sorted(self.priorities.values(), key=lambda x: x["priority"])
        for it in items:
            it["visible"] = False
        if not self.priorities_autoselect and self.manual_priority in self.priorities:
            if self.priorities[self.manual_priority].get("active", True):
                self.priorities[self.manual_priority]["visible"] = True
                return self.priorities[self.manual_priority]
        for it in items:
            if it.get("active", True):
                it["visible"] = True
                return it
        return None

    def _telemetry_loop(self):
        while self.running:
            time.sleep(1.0)
            msg = self._build_perf_msg()
            self.broadcast_ws(msg, only_attr="perf_sub")

            # Push fallback or latest preview image to any WebSocket client with live video enabled
            with self.ws_lock:
                img_clients = [c for c in self.ws_clients if c.get("image_stream")]
            if img_clients and not self.grabber.is_capturing():
                png = self.grabber.generate_fallback_preview_png(self.last_rgb_flat)
                for c in img_clients:
                    try:
                        self._ws_send(c, 0x2, png)
                    except Exception:
                        pass

    def _build_perf_msg(self):
        cfg = self.get_active_config()
        led_cnt = len(cfg.get("leds", []))
        cores = os.cpu_count() or 4
        grab_fps = round(self.grabber.current_fps if self.grabber.is_capturing() else self.smoothing_fps, 1)
        return {
            "command": "performance-update",
            "data": {
                "broadcast": [
                    {"type": 1, "id": 0, "remove": 0, "token": 1, "param1": grab_fps, "param2": self.grabber.last_latency_ms, "param3": self.grabber.active_backend, "param4": 0, "refresh": 5},
                    {"type": 2, "id": self.current_instance, "remove": 0, "token": 1, "name": "First LED instance", "param1": round(self.smoothing_fps, 1), "param2": f"{led_cnt} LEDs", "param3": 0, "param4": 0, "refresh": 5},
                    {"type": 3, "id": self.current_instance, "remove": 0, "token": 1, "name": self.wled.device_type.upper(), "param1": round(self.wled.current_fps, 1), "param2": self.wled.packets_sent, "param3": led_cnt, "param4": self.wled.packets_dropped, "refresh": 5},
                    {"type": 4, "id": 0, "remove": 0, "name": json.dumps({"cores": [0.12] * cores, "total": 12.0})},
                    {"type": 5, "id": 0, "remove": 0, "name": json.dumps({"takenMem": 640, "totalPhysMem": 2048, "aspect": 31})},
                    {"type": 6, "id": 0, "remove": 0, "name": "42.0"},
                ]
            },
        }

    def _flatbuf_server(self):
        self._binary_stream_server(self.flatbuf_port, "FlatBuffers")

    def _proto_server(self):
        self._binary_stream_server(self.proto_port, "Protobuf")

    def _binary_stream_server(self, port, proto_name):
        srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            srv.bind(("0.0.0.0", port))
            srv.listen(8)
            self.log("INFO", proto_name.upper(), f"Started {proto_name} server on 0.0.0.0:{port}")
            while self.running:
                conn, _ = srv.accept()
                threading.Thread(target=self._handle_binary_stream_conn, args=(conn, proto_name), daemon=True).start()
        except Exception as e:
            self.log("ERROR", proto_name.upper(), f"Could not bind {proto_name} port {port}: {e}")

    def _handle_binary_stream_conn(self, conn, proto_name):
        with conn:
            try:
                while self.running:
                    hdr = self._recv_exact(conn, 4)
                    if not hdr:
                        break
                    msg_len = struct.unpack("!I", hdr)[0]
                    if msg_len <= 0 or msg_len > 16 * 1024 * 1024:
                        break
                    msg = self._recv_exact(conn, msg_len)
                    if not msg:
                        break
                    self._parse_external_binary_frame(msg, proto_name)
                    if proto_name == "FlatBuffers":
                        conn.sendall(
                            b"\x00\x00\x00\x10\x08\x00\x00\x00\x00\x00\x06\x00\x08\x00\x00\x00\x06\x00\x00\x00"
                        )
                    else:
                        conn.sendall(b"\x00\x00\x00\x04\x08\x01\x10\x01")
            except Exception:
                pass

    def _parse_external_binary_frame(self, msg, proto_name):
        try:
            n = len(msg)
            if n < 64:
                return
            for off in range(8, min(96, n - 12), 4):
                vec_len = struct.unpack_from("<I", msg, off)[0]
                if vec_len >= 48 and off + 4 + vec_len <= n:
                    w = h = 0
                    bpp = 3
                    if vec_len % 3 == 0:
                        px = vec_len // 3
                        for cw in (64, 80, 96, 128, 160, 192, 240, 256, 320, 384, 480, 640, 720, 960, 1280, 1920):
                            if px % cw == 0:
                                w, h, bpp = cw, px // cw, 3
                                break
                    elif vec_len % 4 == 0:
                        px = vec_len // 4
                        for cw in (64, 80, 96, 128, 160, 192, 240, 256, 320, 384, 480, 640, 720, 960, 1280, 1920):
                            if px % cw == 0:
                                w, h, bpp = cw, px // cw, 4
                                break
                    if 8 <= w <= 3840 and 8 <= h <= 2160:
                        raw_slice = msg[off + 4 : off + 4 + vec_len]
                        if bpp == 4:
                            rgb = bytearray(w * h * 3)
                            for i in range(w * h):
                                rgb[i * 3 : i * 3 + 3] = raw_slice[i * 4 : i * 4 + 3]
                            raw_slice = bytes(rgb)
                        self.grabber.inject_external_frame(raw_slice, w, h, proto_name)
                        return
        except Exception:
            pass

    def _json_rpc_server(self):
        srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            srv.bind(("0.0.0.0", self.json_port))
            srv.listen(16)
            self.log("INFO", "JSONSERVER", f"Started JSON-RPC server on 0.0.0.0:{self.json_port}")
            while self.running:
                conn, _ = srv.accept()
                threading.Thread(target=self._handle_raw_json, args=(conn,), daemon=True).start()
        except Exception as e:
            self.log("ERROR", "JSONSERVER", f"Could not bind JSON-RPC port {self.json_port}: {e}")

    def _handle_raw_json(self, conn):
        with conn:
            f = conn.makefile("r", encoding="utf-8", errors="ignore")
            for line in f:
                line = line.strip()
                if not line:
                    continue
                try:
                    req = json.loads(line)
                    resp = self.handle_rpc(req, None)
                    if resp:
                        conn.sendall((json.dumps(resp) + "\n").encode("utf-8"))
                except Exception:
                    pass

    def _http_ws_server(self):
        srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        srv.bind(("0.0.0.0", self.http_port))
        srv.listen(32)
        lan_ip = get_lan_ip()
        self.log("INFO", "WEBSERVER", f"Started HyperHDR v{VERSION} Web UI & WebSocket server at http://{lan_ip}:{self.http_port}")
        while self.running:
            conn, _ = srv.accept()
            threading.Thread(target=self._handle_http_or_ws, args=(conn,), daemon=True).start()

    def _handle_http_or_ws(self, conn):
        try:
            req_line = self._read_line(conn)
            if not req_line:
                conn.close()
                return
            parts = req_line.split(" ")
            method = parts[0] if parts else "GET"
            raw_path = parts[1] if len(parts) > 1 else "/"

            headers = {}
            while True:
                hline = self._read_line(conn)
                if not hline:
                    break
                if ":" in hline:
                    k, v = hline.split(":", 1)
                    headers[k.strip().lower()] = v.strip()

            if "websocket" in headers.get("upgrade", "").lower() and "sec-websocket-key" in headers:
                self._serve_ws(conn, headers["sec-websocket-key"])
                return

            path = raw_path.split("?", 1)[0]
            if method.upper() == "POST" and path in ("/json-rpc", "/jsonrpc"):
                clen = int(headers.get("content-length", "0"))
                body = self._recv_exact(conn, clen) if clen > 0 else b"{}"
                resp = self.handle_rpc(json.loads(body.decode("utf-8")), None)
                self._send_http(conn, "200 OK", "application/json", json.dumps(resp or {}).encode("utf-8"))
                conn.close()
                return

            if path == "/hyperhdr_heart_beat":
                self._send_http(conn, "200 OK", "text/plain", b"OK")
                conn.close()
                return

            if path in ("/", ""):
                path = "/index.html"
            rel = path.lstrip("/")
            real_www = os.path.realpath(self.www_dir)
            full = os.path.realpath(os.path.join(self.www_dir, rel))
            if not full.startswith(real_www + os.sep) and full != real_www:
                self._send_http(conn, "403 Forbidden", "text/plain", b"Forbidden")
                conn.close()
                return

            if os.path.isfile(full):
                with open(full, "rb") as f:
                    data = f.read()
                if rel == "content/about.html":
                    data = data.replace(
                        b"https://raw.githubusercontent.com/awawa-dev/HyperHDR/master/3RD_PARTY_LICENSES",
                        b"/3RD_PARTY_LICENSES",
                    )
                ctype = mimetypes.guess_type(full)[0] or "application/octet-stream"
                self._send_http(conn, "200 OK", ctype, data)
            else:
                self._send_http(conn, "404 Not Found", "text/plain", b"Not Found")
            conn.close()
        except Exception:
            try:
                conn.close()
            except Exception:
                pass

    @staticmethod
    def _read_line(conn):
        buf = bytearray()
        while True:
            b = conn.recv(1)
            if not b:
                break
            if b == b"\n":
                break
            if b != b"\r":
                buf.extend(b)
                if len(buf) > 8192:
                    raise ValueError("HTTP request line exceeded 8KB limit")
        return buf.decode("ascii", errors="ignore")

    @staticmethod
    def _send_http(conn, status, ctype, body):
        hdr = (
            f"HTTP/1.1 {status}\r\n"
            f"Content-Type: {ctype}\r\n"
            f"Content-Length: {len(body)}\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: close\r\n\r\n"
        ).encode("ascii")
        conn.sendall(hdr + body)

    def _serve_ws(self, conn, ws_key):
        accept = base64.b64encode(
            hashlib.sha1((ws_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode("ascii")).digest()
        ).decode("ascii")
        handshake = (
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            f"Sec-WebSocket-Accept: {accept}\r\n\r\n"
        ).encode("ascii")
        conn.sendall(handshake)

        client = {
            "conn": conn, "lock": threading.Lock(),
            "authenticated": False, "session_token": None,
            "led_stream": False, "image_stream": False,
            "logging_stream": False, "perf_sub": False,
        }
        with self.ws_lock:
            self.ws_clients.append(client)

        try:
            while self.running:
                hdr = self._recv_exact(conn, 2)
                if not hdr:
                    break
                opcode = hdr[0] & 0x0F
                masked = (hdr[1] & 0x80) != 0
                plen = hdr[1] & 0x7F
                if plen == 126:
                    plen = struct.unpack("!H", self._recv_exact(conn, 2))[0]
                elif plen == 127:
                    plen = struct.unpack("!Q", self._recv_exact(conn, 8))[0]
                mask = self._recv_exact(conn, 4) if masked else b""
                payload = bytearray(self._recv_exact(conn, plen))
                if masked:
                    for i in range(plen):
                        payload[i] ^= mask[i & 3]
                if opcode == 0x8:
                    break
                elif opcode == 0x9:
                    self._ws_send(client, 0xA, bytes(payload))
                elif opcode == 0x1:
                    text = payload.decode("utf-8", errors="ignore").strip()
                    for line in text.splitlines():
                        line = line.strip()
                        if not line:
                            continue
                        resp = self.handle_rpc(json.loads(line), client)
                        if resp:
                            self._ws_send(client, 0x1, json.dumps(resp).encode("utf-8"))
        except Exception:
            pass
        finally:
            with self.ws_lock:
                if client in self.ws_clients:
                    self.ws_clients.remove(client)
            try:
                conn.close()
            except Exception:
                pass

    @staticmethod
    def _recv_exact(conn, n):
        buf = bytearray()
        while len(buf) < n:
            chunk = conn.recv(n - len(buf))
            if not chunk:
                return None
            buf.extend(chunk)
        return bytes(buf)

    @staticmethod
    def _ws_send(client, opcode, data):
        with client["lock"]:
            n = len(data)
            if n <= 125:
                hdr = bytes([0x80 | opcode, n])
            elif n <= 65535:
                hdr = struct.pack("!BBH", 0x80 | opcode, 126, n)
            else:
                hdr = struct.pack("!BBQ", 0x80 | opcode, 127, n)
            client["conn"].sendall(hdr + data)

    def broadcast_ws(self, obj, only_attr=None):
        raw = json.dumps(obj).encode("utf-8")
        with self.ws_lock:
            clients = list(self.ws_clients)
        for c in clients:
            if only_attr and not c.get(only_attr):
                continue
            try:
                self._ws_send(c, 0x1, raw)
            except Exception:
                pass

    def handle_rpc(self, req, client):
        cmd = req.get("command", "")
        sub = req.get("subcommand", "")
        tan = req.get("tan", 1)
        full_cmd = f"{cmd}-{sub}" if sub else cmd
        resp = {"command": full_cmd, "success": True, "tan": tan}

        if cmd != "authorize":
            if client is not None:
                if not client.get("authenticated", False):
                    return {"command": full_cmd, "success": False, "error": "No Authorization", "tan": tan}
            else:
                # Non-WebSocket request (HTTP POST /json-rpc or raw TCP 19444)
                req_token = req.get("token")
                current_pw = self.db.get("adminPassword", "hyperhdr")
                is_default_pw = bool(self.db.get("defaultPasswordSet", True)) and (current_pw == "hyperhdr")
                if not is_default_pw:
                    saved_toks = {t.get("token") for t in self.db.get("tokens", []) if isinstance(t, dict)}
                    token_valid = req_token and (req_token in self.active_session_tokens or req_token in saved_toks)
                    if not token_valid and cmd not in ("serverinfo", "sysinfo"):
                        return {"command": full_cmd, "success": False, "error": "No Authorization", "tan": tan}

        if cmd == "authorize":
            current_pw = self.db.get("adminPassword", "hyperhdr")
            is_default_pw = bool(self.db.get("defaultPasswordSet", True)) and (current_pw == "hyperhdr")

            if sub == "adminRequired":
                resp["info"] = {"adminRequired": True}
            elif sub == "newPasswordRequired":
                resp["info"] = {"newPasswordRequired": is_default_pw}
            elif sub == "login":
                pw = req.get("password")
                tok = req.get("token")
                ok = False
                issued_token = None
                if pw is not None:
                    if pw == current_pw:
                        ok = True
                        issued_token = str(uuid.uuid4())
                        self.active_session_tokens.add(issued_token)
                elif tok is not None:
                    saved_toks = {t.get("token") for t in self.db.get("tokens", []) if isinstance(t, dict)}
                    if tok in self.active_session_tokens or tok in saved_toks:
                        ok = True
                        issued_token = tok
                if ok:
                    if client is not None:
                        client["authenticated"] = True
                        client["session_token"] = issued_token
                    resp["info"] = {"token": issued_token}
                else:
                    if client is not None:
                        client["authenticated"] = False
                    resp["success"] = False
                    resp["error"] = "No Authorization"
            elif sub == "logout":
                if client is not None:
                    if client.get("session_token"):
                        self.active_session_tokens.discard(client["session_token"])
                    client["authenticated"] = False
                    client["session_token"] = None
            elif sub == "newPassword":
                if client is not None and not client.get("authenticated", False):
                    resp["success"] = False
                    resp["error"] = "No Authorization"
                    return resp
                old_pw = req.get("password", "")
                new_pw = str(req.get("newPassword", "")).strip()
                if old_pw != current_pw or len(new_pw) < 8:
                    resp["success"] = False
                    resp["error"] = "Wrong current password or new password shorter than 8 characters"
                    return resp
                self.db["adminPassword"] = new_pw
                self.db["defaultPasswordSet"] = (new_pw == "hyperhdr")
                self.active_session_tokens.clear()
                self._save_db()
                self.log("INFO", "AUTH", "Admin password updated; previous session tokens invalidated")
            elif sub == "getTokenList":
                resp["info"] = self.db.get("tokens", [])
            elif sub == "getPendingTokenRequests":
                resp["info"] = []
            elif sub == "createToken":
                tok_obj = {
                    "id": str(uuid.uuid4())[:8],
                    "comment": req.get("comment", "App Token"),
                    "last_use": time.strftime("%Y-%m-%d %H:%M:%S"),
                    "token": str(uuid.uuid4()),
                }
                self.db.setdefault("tokens", []).append(tok_obj)
                self._save_db()
                resp["info"] = tok_obj
            elif sub == "deleteToken":
                del_id = req.get("id", "")
                self.db["tokens"] = [t for t in self.db.get("tokens", []) if t.get("id") != del_id]
                self._save_db()
        elif cmd == "config":
            if sub == "getschema":
                resp["info"] = self.server_schema
            elif sub == "getconfig":
                resp["info"] = self.get_active_config()
            elif sub == "setconfig":
                new_cfg = req.get("config", {})
                cur_cfg = self.get_active_config()
                cur_cfg.update(new_cfg)
                self._save_db()
                self.wled.configure(cur_cfg.get("device", {}))
                self.grabber.configure(cur_cfg.get("systemGrabber", {}))
                if "systemControl" in cur_cfg:
                    sys_en = bool(cur_cfg["systemControl"].get("systemInstanceEnable", True))
                    self.components["SYSTEMGRABBER"] = sys_en
                    self.grabber.enabled = sys_en
                if any(k in new_cfg for k in ("systemGrabber", "videoGrabber", "systemControl", "videoControl")):
                    if not self.grabber.is_capturing():
                        threading.Thread(target=self.grabber.request_mediaprojection_permission, daemon=True).start()
                self.log("INFO", "CONFIG", f"Configuration saved (Grabber: Android MediaProjection API, LED device: {self.wled.device_type} @ {self.wled.host})")
                self.broadcast_ws({"command": "settings-update", "data": cur_cfg})
        elif cmd == "mediaprojection":
            if sub == "stop":
                self.grabber.enabled = False
                self.log("INFO", "GRABBER", "Android MediaProjection API capture stopped")
            else:
                self.grabber.enabled = True
                ok = self.grabber.request_mediaprojection_permission()
                self.log(
                    "INFO",
                    "GRABBER",
                    "Requested Android MediaProjection API permission via companion activity"
                    if ok
                    else "Android MediaProjection API active (ensure HyperHDR Android TV APK is installed for MediaProjection dialog)",
                )
            resp["info"] = {"active": self.grabber.is_capturing(), "mode": self.grabber.active_backend}
        elif cmd == "sysinfo":
            resp["info"] = {
                "hyperhdr": {
                    "version": VERSION,
                    "build": BUILD_STR,
                    "time": BUILD_TIME,
                    "gitremote": "https://github.com/awawa-dev/HyperHDR",
                    "channel": "Beta",
                    "readOnlyMode": False,
                },
                "system": {
                    "productType": "linux",
                    "prettyName": f"Termux Headless ({platform.platform()})",
                    "architecture": platform.machine(),
                    "cpuModelName": platform.processor() or "ARM64 SoC",
                    "cpuModelType": platform.machine(),
                    "cpuHardware": "Android / Termux",
                    "kernelType": "linux",
                    "kernelVersion": platform.release(),
                    "wordSize": "64",
                    "qtVersion": "6.8.4",
                    "openssl": "OpenSSL 3.x",
                },
            }
        elif cmd == "serverinfo":
            if client and "performance-update" in req.get("subscribe", []):
                client["perf_sub"] = True
            resp["info"] = self._build_serverinfo()
        elif cmd == "componentstate":
            cs = req.get("componentstate", {})
            cname = cs.get("component", "ALL")
            cstate = bool(cs.get("state", True))
            self.components[cname] = cstate
            if cname in ("GRABBER", "SYSTEMGRABBER"):
                self.components["GRABBER"] = cstate
                self.components["SYSTEMGRABBER"] = cstate
                self.grabber.enabled = cstate
            elif cname == "ALL":
                self.wled.enabled = cstate and self.components.get("LEDDEVICE", True)
            elif cname == "LEDDEVICE":
                self.wled.enabled = cstate and self.components.get("ALL", True)
            self.broadcast_ws({"command": "components-update", "data": {"name": cname, "enabled": cstate}})
            self._broadcast_prios()
        elif cmd == "ledcolors":
            if client:
                if sub == "ledstream-start":
                    client["led_stream"] = True
                elif sub == "ledstream-stop":
                    client["led_stream"] = False
                elif sub == "imagestream-start":
                    client["image_stream"] = True
                    png = self.grabber.generate_fallback_preview_png(self.last_rgb_flat)
                    if png:
                        self._ws_send(client, 0x2, png)
                elif sub == "imagestream-stop":
                    client["image_stream"] = False
        elif cmd == "leddevice":
            if sub == "discover":
                resp["info"] = {"ledDeviceType": req.get("ledDeviceType", "wled"), "devices": self.wled.discover_devices()}
            elif sub == "identify":
                params = req.get("params", {})
                self.wled.identify(params.get("host"))
            elif sub == "hasLedClock":
                resp["info"] = {"hasLedClock": 0}
        elif cmd == "color":
            c = req.get("color", [255, 0, 0])
            prio = int(req.get("priority", 1))
            dur = int(req.get("duration", -1))
            self.priorities[prio] = {
                "priority": prio, "componentId": "COLOR",
                "origin": req.get("origin", "Web Configuration"),
                "owner": "Solid Color", "active": True, "visible": True,
                "duration_ms": dur, "expires_at": (time.time() + dur / 1000.0) if dur > 0 else None,
                "value": {"RGB": c[:3]},
            }
            self._broadcast_prios()
        elif cmd == "image":
            b64 = req.get("imagedata", "")
            if "," in b64:
                b64 = b64.split(",", 1)[1]
            if b64:
                try:
                    raw = base64.b64decode(b64)
                    iw = int(req.get("imagewidth", 0))
                    ih = int(req.get("imageheight", 0))
                    prio = int(req.get("priority", 1))
                    dur = int(req.get("duration", -1))
                    if iw > 0 and ih > 0 and len(raw) >= iw * ih * 3:
                        if len(raw) >= iw * ih * 4:
                            rgb = bytearray(iw * ih * 3)
                            for i in range(iw * ih):
                                rgb[i * 3 : i * 3 + 3] = raw[i * 4 : i * 4 + 3]
                            raw = bytes(rgb)
                        self.grabber.inject_external_frame(raw, iw, ih, "JSON-RPC Image")
                        self.priorities[prio] = {
                            "priority": prio, "componentId": "IMAGE",
                            "origin": req.get("origin", "Web Configuration"),
                            "owner": req.get("name", "Image"), "active": True, "visible": True,
                            "duration_ms": dur, "expires_at": (time.time() + dur / 1000.0) if dur > 0 else None,
                            "image_rgb": raw, "image_w": iw, "image_h": ih,
                        }
                        self._broadcast_prios()
                except Exception:
                    pass
        elif cmd == "effect":
            eff = req.get("effect", {}).get("name", "Rainbow swirl")
            prio = int(req.get("priority", 1))
            dur = int(req.get("duration", -1))
            self.priorities[prio] = {
                "priority": prio, "componentId": "EFFECT",
                "origin": req.get("origin", "Web Configuration"),
                "owner": eff, "active": True, "visible": True,
                "duration_ms": dur, "expires_at": (time.time() + dur / 1000.0) if dur > 0 else None,
            }
            self._broadcast_prios()
        elif cmd == "sourceselect":
            if req.get("auto", False):
                self.priorities_autoselect = True
            else:
                self.priorities_autoselect = False
                self.manual_priority = int(req.get("priority", 250))
            self._broadcast_prios()
        elif cmd == "processing":
            self.mapping_type = req.get("mappingType", "multicolor_mean")
            self.broadcast_ws({"command": "imageToLedMapping-update", "data": {"imageToLedMappingType": self.mapping_type}})
        elif cmd == "clear":
            prio = int(req.get("priority", 1))
            if prio < 0:
                for k in list(self.priorities.keys()):
                    if k < 250:
                        del self.priorities[k]
            elif prio in self.priorities and prio < 250:
                del self.priorities[prio]
            self._broadcast_prios()
        elif cmd == "clearall":
            for k in list(self.priorities.keys()):
                if k < 250:
                    del self.priorities[k]
            self._broadcast_prios()
        elif cmd == "logging":
            if client:
                if sub == "start":
                    client["logging_stream"] = True
                    self._ws_send(client, 0x1, json.dumps({"command": "logging-update", "result": {"messages": self.logs}}).encode("utf-8"))
                elif sub == "stop":
                    client["logging_stream"] = False
        elif cmd == "performance-counters":
            if client:
                client["perf_sub"] = True
                self._ws_send(client, 0x1, json.dumps(self._build_perf_msg()).encode("utf-8"))
        elif cmd == "signal-calibration":
            resp["info"] = {
                "running": False, "isSuccess": True,
                "model": "Termux Android Screen Grabber sRGB/BT.709",
                "quality": "100%", "sdrStat": "OK", "sdrPoint": "100",
                "hdrStat": "OK", "hdrPoint": "100", "status": "Completed",
            }
        elif cmd == "save-db":
            resp["info"] = self.db
        elif cmd == "load-db":
            if "config" in req:
                self.db = req["config"]
                self._save_db()
        elif cmd == "instance":
            instances = self.db.setdefault("instances", [])
            target_inst = int(req.get("instance", 0))
            if sub == "switchTo":
                self.current_instance = target_inst
                self.db["currentInstance"] = self.current_instance
                self._save_db()
                cfg = self.get_active_config()
                self.wled.configure(cfg.get("device", {}))
                self.grabber.configure(cfg.get("systemGrabber", {}))
                return resp
            elif sub == "saveName":
                new_name = req.get("name", "LED Instance")
                for o in instances:
                    if o.get("instance") == target_inst:
                        o["friendly_name"] = new_name
            elif sub in ("startInstance", "stopInstance"):
                run_state = (sub == "startInstance")
                for o in instances:
                    if o.get("instance") == target_inst:
                        o["running"] = run_state
            elif sub == "createInstance":
                name = req.get("name", "Secondary LED instance")
                next_id = max([o.get("instance", 0) for o in instances] + [0]) + 1
                instances.append({"instance": next_id, "friendly_name": name, "running": True})
                self.db.setdefault("settings", {})[str(next_id)] = copy.deepcopy(self.default_config)
            elif sub == "deleteInstance" and target_inst > 0:
                self.db["instances"] = [o for o in instances if o.get("instance") != target_inst]
                self.db.setdefault("settings", {}).pop(str(target_inst), None)
                if self.current_instance == target_inst:
                    self.current_instance = 0
                    self.db["currentInstance"] = 0
            self._save_db()
            self.broadcast_ws({"command": "instance-update", "data": self.db.get("instances", [])})
        return resp

    def _public_priorities(self):
        self._get_visible_priority()
        out = []
        for p in sorted(self.priorities.values(), key=lambda x: x["priority"]):
            item = {k: v for k, v in p.items() if k not in ("image_rgb", "image_w", "image_h", "expires_at")}
            out.append(item)
        return out

    def _broadcast_prios(self):
        self.broadcast_ws({
            "command": "priorities-update",
            "data": {"priorities": self._public_priorities(), "priorities_autoselect": self.priorities_autoselect},
        })

    def _build_serverinfo(self):
        cfg = self.get_active_config()
        ca = cfg.get("color", {}).get("channelAdjustment", [])
        return {
            "currentInstance": self.current_instance,
            "lastError": "",
            "hasCEC": 1,
            "hasPROTOBUF": 1,
            "imageToLedMappingType": self.mapping_type,
            "videomode": self.videomode,
            "videomodehdr": self.videomodehdr,
            "components": [{"name": k, "enabled": v} for k, v in self.components.items()],
            "adjustment": ca,
            "priorities": self._public_priorities(),
            "priorities_autoselect": self.priorities_autoselect,
            "instance": self.db.get("instances", []),
            "ledDevices": {
                "available": [
                    {"name": n, "group": "leds_group_0_SPI"}
                    for n in ("apa102", "apa104", "hd108", "hyperspi", "lpd6803", "lpd8806", "p9813", "sk6812spi", "sk6822spi", "sk9822", "ws2801", "ws2812spi")
                ] + [
                    {"name": n, "group": "leds_group_1_PWM"}
                    for n in ("piblaster", "rpi_pio", "ws281x")
                ] + [
                    {"name": n, "group": "leds_group_2_network"}
                    for n in ("wled", "hyperk", "ddp", "philipshue", "wiz", "lifx", "home_assistant", "zigbee2mqtt", "nanoleaf", "yeelight", "cololight", "atmoorb", "fadecandy", "tinkerforge", "udpe131", "udpartnet", "udph801", "udpraw", "tpm2net")
                ] + [
                    {"name": n, "group": "leds_group_3_serial"}
                    for n in ("adalight", "dmx", "karate", "skydimo", "sp616e", "sedu", "tpm2", "atmo")
                ] + [
                    {"name": n, "group": "leds_group_6_HID"}
                    for n in ("lightpack", "paintpack", "rawhid", "synclight")
                ] + [{"name": "file", "group": "leds_group_5_debug"}]
            },
            "grabbers": {
                "active": [f"Android MediaProjection API ({self.grabber.active_backend})", "Android MediaProjection API"],
                "available": [
                    "System capturing (Android MediaProjection API)",
                    "Video capturing (Android MediaProjection API)",
                ],
                "lut_for_hdr_exists": 1,
                "lutFastCRC": "0x240000B0",
                "current": {
                    "device": f"Android MediaProjection API ({self.grabber.active_backend})",
                    "videoMode": f"{self.grabber.target_width}x{self.grabber.target_height} {self.grabber.target_fps}fps RGB24 (MediaProjection)",
                },
                "video_devices": [
                    {
                        "device": "Android MediaProjection API",
                        "name": "Android MediaProjection API (Hardware VirtualDisplay Screen Grabber)",
                        "videoControls": {
                            "BrightnessDef": "Brightness: 0",
                            "ContrastDef": "Contrast: 0",
                            "SaturationDef": "Saturation: 0",
                            "HueDef": "Hue: 0",
                        },
                        "videoModeList": [
                            {"width": 256, "height": 144, "fps": 60, "pixel_format_info": "RGBA"},
                            {"width": 256, "height": 144, "fps": 30, "pixel_format_info": "RGBA"},
                            {"width": 256, "height": 144, "fps": 25, "pixel_format_info": "RGBA"},
                            {"width": 320, "height": 180, "fps": 60, "pixel_format_info": "RGBA"},
                            {"width": 320, "height": 180, "fps": 30, "pixel_format_info": "RGBA"},
                            {"width": 640, "height": 360, "fps": 60, "pixel_format_info": "RGBA"},
                            {"width": 640, "height": 360, "fps": 30, "pixel_format_info": "RGBA"},
                            {"width": 1280, "height": 720, "fps": 60, "pixel_format_info": "RGBA"},
                            {"width": 1280, "height": 720, "fps": 30, "pixel_format_info": "RGBA"},
                            {"width": 1920, "height": 1080, "fps": 60, "pixel_format_info": "RGBA"},
                            {"width": 1920, "height": 1080, "fps": 30, "pixel_format_info": "RGBA"},
                        ],
                        "framerates": ["60", "30", "25"],
                        "inputs": [],
                    }
                ],
            },
            "grabberstate": {
                "device": f"Android MediaProjection API ({self.grabber.active_backend})",
                "videoMode": f"{self.grabber.target_width}x{self.grabber.target_height} {self.grabber.target_fps}fps RGB24 (MediaProjection)",
            },
            "systemGrabbers": {
                "device": "Android MediaProjection API",
                "modes": [
                    "Android MediaProjection API",
                    "Android MediaProjection API (Low-Latency 256x144)",
                    "Android MediaProjection API (HD 320x180)",
                    "Android MediaProjection API (Full 640x360)",
                    "Termux Android screencap / ADB",
                ],
            },
            "effects": [
                {"name": n}
                for n in ("Rainbow swirl", "Rainbow swirl fast", "Warm mood blobs", "Full color mood blobs", "Atomic swirl", "Sea waves", "Plasma", "Breath", "Candle", "Knight rider", "Police Lights Single", "Strobe white")
            ],
            "sound": {"sound_available": ["Termux Audio"]},
            "sessions": [{"name": f"WLED ({self.wled.device_type.upper()})", "address": self.wled.host, "port": 80, "host": "WLED LED Controller"}],
        }


def main():
    parser = argparse.ArgumentParser(prog="hyperhdr", description="HyperHDR v24.0.0-beta.0 Headless Server for Termux")
    parser.add_argument("-v", "--version", action="version", version=f"HyperHDR {VERSION} ({BUILD_STR})")
    parser.add_argument("-d", "--debug", action="store_true", help="Enable debug output")
    parser.add_argument("--desktop", action="store_true", help="Desktop/headless compatibility flag")
    parser.add_argument("-u", "--userdata", default=os.path.expanduser("~/.hyperhdr"), help="Overwrite user data path (default: ~/.hyperhdr)")
    parser.add_argument("--port", type=int, default=None, help="Override Web UI HTTP/WebSocket port (default: 8090)")
    parser.add_argument("--json-port", type=int, default=None, help="Override JSON-RPC TCP port (default: 19444)")
    parser.add_argument("--wled", default=None, help="Set target WLED IP/hostname")
    parser.add_argument("--assets", default=os.path.expanduser("$PREFIX/opt/hyperhdr/share/hyperhdr"), help="Path to extracted HyperHDR web/schema assets")
    args, _ = parser.parse_known_args()

    daemon = HeadlessHyperHdrDaemon(
        assets_dir=args.assets,
        userdata_dir=args.userdata,
        web_port=args.port,
        json_port=args.json_port,
        wled_host=args.wled,
    )
    daemon.start()


if __name__ == "__main__":
    main()
