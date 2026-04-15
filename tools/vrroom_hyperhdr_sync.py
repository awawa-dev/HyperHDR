#!/usr/bin/env python3
"""
vrroom_hyperhdr_sync.py — VRRoom ↔ HyperHDR signal detection and LUT switching bridge.

Polls a Vertex2/VRRoom HDMI matrix for video signal status (SDR / HDR / Dolby Vision)
and pushes the classified mode to HyperHDR so the runtime can select the correct LUT
and toggle HDR tone mapping automatically.

When the active source is Dolby Vision, also polls Kodi's JSON-RPC API for per-scene
RPU metadata (L1 / L6 nits) used by HyperHDR's FLL-based DV runtime LUT bucket model.

Features:
  - VRRoom TX status classification: SDR, HDR10, HLG, LLDV (low-latency Dolby Vision)
  - AVI / HDR / VSI / HVS infoframe parsing (colorimetry, EOTF, DV VSVDB)
  - Dolby Vision sink capability parsing from EDID extended blocks
  - Kodi DV RPU metadata polling (L1 min/max/avg, source min/max, L6 MaxCLL/MaxFALL)
  - Per-input device name resolution with HTTP override API
  - Debounced mode classification with configurable stable-poll threshold
  - Full video-stream-report JSON pushed to HyperHDR's current-state API

Requirements:
  - Python 3.10+
  - Network access to the VRRoom HTTP/TCP interface, HyperHDR JSON-RPC, and (optionally) Kodi JSON-RPC
  - LUT files placed in ~/.hyperhdr/ (see LUT_DIR below)

Usage:
  1. Edit the User Config section below with your network addresses.
  2. Place your LUT .3d files in ~/.hyperhdr/ and adjust the LUT config paths.
  3. Run:  python3 vrroom_hyperhdr_sync.py
"""
import base64
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import math
import re
import socket
import sys
import threading
import time
import urllib.parse
import urllib.request
from pathlib import Path
from typing import Optional

# =========================
# User config
# =========================
VRROOM_HOST = "YOUR_VRROOM_IP"        # e.g. "192.168.1.100"
VRROOM_PORT = 2222
STATUS_TARGET = "tx1"                  # "tx0" or "tx1" — which VRRoom output to monitor

HYPERHDR_HOST = "127.0.0.1"           # localhost when running on the same Pi as HyperHDR
HYPERHDR_PORT = 8090

KODI_HOST = "YOUR_KODI_IP"            # e.g. "192.168.1.110" — leave empty to disable Kodi polling
KODI_PORT = 8080
KODI_USERNAME = "kodi"
KODI_PASSWORD = "kodi"
KODI_TIMEOUT = 5.0
KODI_INFO_LABELS = [
    "Player.Process(video.dovi.l1.min.nits)",
    "Player.Process(video.dovi.l1.max.nits)",
    "Player.Process(video.dovi.l1.avg.nits)",
    "Player.Process(video.dovi.source.min.nits)",
    "Player.Process(video.dovi.source.max.nits)",
    "Player.Process(video.dovi.l6.min.lum)",
    "Player.Process(video.dovi.l6.max.lum)",
    "Player.Process(video.dovi.l6.max.cll)",
    "Player.Process(video.dovi.l6.max.fall)",
]

VRROOM_POLL_SECONDS = 2.0
KODI_DV_POLL_SECONDS = 0.0416
LOOP_SLEEP_SECONDS = 0.05
SOCKET_TIMEOUT = 3.0
DEBUG = True
DEBOUNCE_POLLS = 2
INFO_LOG_INTERVAL_POLLS = 15

TRANSFER_PROFILE_SDR = "transfer_curve_bt1886_500nit_6227"
TRANSFER_PROFILE_HLG = "transfer_curve_hlg_1500nits"
TRANSFER_PROFILE_PQ = "transfer_curve_pq_shadowlift100_shoulder100_1500nits_6227"

# Map VRRoom RX inputs → friendly device names used by HyperHDR for per-input LUT overrides.
INPUT_DEVICE_NAMES = {
    "rx0": "device0",
    "rx1": "device1",
    "rx2": "device2",
    "rx3": "device3",
}

# Per-input override variants selectable via the HTTP override API.
# Example: rx2 with override mode "moonlight" resolves to "device2moonlight".
INPUT_DEVICE_OVERRIDE_NAMES = {
    "rx2": {
        "moonlight": "device2moonlight",
    },
}

INPUT_OVERRIDE_LISTEN_HOST = "0.0.0.0"
INPUT_OVERRIDE_LISTEN_PORT = 8765
INPUT_OVERRIDE_TOKEN = ""              # set a shared secret to require auth on the override API

# =========================
# LUT config
# =========================
LUT_DIR = Path("/home/pi/.hyperhdr")
ACTIVE_LUT = LUT_DIR / "lut_lin_tables.3d"
SDR_LUT = LUT_DIR / "lut_lin_tables_sdr.3d"
HDR_LUT = LUT_DIR / "lut_lin_tables_hdr.3d"
LLDV_LUT = LUT_DIR / "lut_lin_tables_lldv.3d"
INPUT_OVERRIDE_STATE_FILE = LUT_DIR / "vrroom_input_overrides.json"

NUMERIC_PATTERN = re.compile(r"-?[0-9]+(?:\.[0-9]+)?")

LUT_SWITCHING_ACTIVE_FILE = ACTIVE_LUT.name
LUT_SWITCHING_SDR_FILE = SDR_LUT.name
LUT_SWITCHING_HDR_FILE = HDR_LUT.name
LUT_SWITCHING_LLDV_FILE = LLDV_LUT.name
LUT_SWITCHING_SDR_USES_HDR_FILE = True

_input_override_lock = threading.Lock()
_input_override_modes: dict[str, str] = {}

# =========================
# Helpers
# =========================

def log(msg: str) -> None:
    print(msg, flush=True)

def load_input_override_state() -> None:
    global _input_override_modes
    if not INPUT_OVERRIDE_STATE_FILE.exists():
        _input_override_modes = {}
        return
    try:
        data = json.loads(INPUT_OVERRIDE_STATE_FILE.read_text(encoding="utf-8"))
        if isinstance(data, dict):
            _input_override_modes = {str(k): str(v) for k, v in data.items() if isinstance(k, str) and isinstance(v, str)}
        else:
            _input_override_modes = {}
    except Exception as exc:
        _input_override_modes = {}
        log(f"Warning: could not load input override state: {exc}")

def save_input_override_state() -> None:
    try:
        INPUT_OVERRIDE_STATE_FILE.write_text(json.dumps(_input_override_modes, indent=2) + "\n", encoding="utf-8")
    except Exception as exc:
        log(f"Warning: could not save input override state: {exc}")

def get_input_override_mode(input_name: str) -> Optional[str]:
    with _input_override_lock:
        return _input_override_modes.get(input_name)

def set_input_override_mode(input_name: str, override_mode: Optional[str]) -> None:
    with _input_override_lock:
        if override_mode:
            _input_override_modes[input_name] = override_mode
        else:
            _input_override_modes.pop(input_name, None)
        save_input_override_state()

def resolve_device_name_for_input(active_input: Optional[str]) -> Optional[str]:
    if not active_input:
        return None
    base_name = INPUT_DEVICE_NAMES.get(active_input, active_input)
    override_mode = get_input_override_mode(active_input)
    variant_names = INPUT_DEVICE_OVERRIDE_NAMES.get(active_input, {})
    if override_mode and override_mode in variant_names:
        return variant_names[override_mode]
    return base_name

def parse_input_override_request(payload: dict) -> tuple[str, Optional[str]]:
    input_name = str(payload.get("input", "") or "").strip().lower() or "rx2"
    if input_name not in INPUT_DEVICE_NAMES:
        raise ValueError(f"Unsupported input: {input_name}")

    override_mode = str(payload.get("mode", "") or "").strip().lower()
    device_name = str(payload.get("device", "") or "").strip().lower()

    if not override_mode and device_name:
        if device_name == INPUT_DEVICE_NAMES.get(input_name, ""):
            override_mode = "default"
        else:
            for mode_name, mode_device in INPUT_DEVICE_OVERRIDE_NAMES.get(input_name, {}).items():
                if device_name == mode_device.lower():
                    override_mode = mode_name
                    break

    if override_mode in {"", "default", "native", "clear", "off"}:
        return input_name, None

    if override_mode in INPUT_DEVICE_OVERRIDE_NAMES.get(input_name, {}):
        return input_name, override_mode

    raise ValueError(f"Unsupported override mode '{override_mode}' for {input_name}")

class InputOverrideRequestHandler(BaseHTTPRequestHandler):
    server_version = "VRRoomInputOverride/1.0"

    def log_message(self, format: str, *args) -> None:
        if DEBUG:
            log("Override API: " + (format % args))

    def _send_json(self, status_code: int, payload: dict) -> None:
        body = (json.dumps(payload, separators=(",", ":")) + "\n").encode("utf-8")
        self.send_response(status_code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _check_token(self, payload: dict) -> bool:
        if not INPUT_OVERRIDE_TOKEN:
            return True
        return str(payload.get("token", "") or "") == INPUT_OVERRIDE_TOKEN

    def do_GET(self) -> None:
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path != "/input-override":
            self._send_json(404, {"ok": False, "error": "not found"})
            return
        params = urllib.parse.parse_qs(parsed.query)
        input_name = str(params.get("input", ["rx2"])[0] or "rx2").strip().lower()
        self._send_json(
            200,
            {
                "ok": True,
                "input": input_name,
                "override_mode": get_input_override_mode(input_name),
                "device": resolve_device_name_for_input(input_name),
            },
        )

    def do_POST(self) -> None:
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path != "/input-override":
            self._send_json(404, {"ok": False, "error": "not found"})
            return

        try:
            content_length = int(self.headers.get("Content-Length", "0") or "0")
        except ValueError:
            content_length = 0
        raw_body = self.rfile.read(content_length) if content_length > 0 else b"{}"
        try:
            payload = json.loads(raw_body.decode("utf-8", errors="replace") or "{}")
        except json.JSONDecodeError:
            self._send_json(400, {"ok": False, "error": "invalid json"})
            return

        if not isinstance(payload, dict):
            self._send_json(400, {"ok": False, "error": "payload must be an object"})
            return
        if not self._check_token(payload):
            self._send_json(403, {"ok": False, "error": "invalid token"})
            return

        try:
            input_name, override_mode = parse_input_override_request(payload)
        except ValueError as exc:
            self._send_json(400, {"ok": False, "error": str(exc)})
            return

        set_input_override_mode(input_name, override_mode)
        resolved_device = resolve_device_name_for_input(input_name)
        log(f"Updated input override: {input_name} -> {override_mode or 'default'} ({resolved_device})")
        self._send_json(
            200,
            {
                "ok": True,
                "input": input_name,
                "override_mode": override_mode,
                "device": resolved_device,
            },
        )

def start_input_override_server() -> Optional[ThreadingHTTPServer]:
    try:
        server = ThreadingHTTPServer((INPUT_OVERRIDE_LISTEN_HOST, INPUT_OVERRIDE_LISTEN_PORT), InputOverrideRequestHandler)
    except Exception as exc:
        log(f"Warning: could not start input override server: {exc}")
        return None

    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    log(f"Input override server listening on {INPUT_OVERRIDE_LISTEN_HOST}:{INPUT_OVERRIDE_LISTEN_PORT}")
    return server

def find_key(obj, wanted):
    if isinstance(obj, dict):
        for k, v in obj.items():
            if k == wanted:
                return v
            found = find_key(v, wanted)
            if found is not None:
                return found
    elif isinstance(obj, list):
        for item in obj:
            found = find_key(item, wanted)
            if found is not None:
                return found
    return None

def http_get_json(url: str, timeout: float = 3.0, headers: Optional[dict[str, str]] = None) -> dict:
    request = urllib.request.Request(url, headers=headers or {})
    with urllib.request.urlopen(request, timeout=timeout) as response:
        body = response.read().decode("utf-8", errors="replace")
    return json.loads(body)

def http_get_bytes(url: str, timeout: float = 3.0) -> bytes:
    with urllib.request.urlopen(url, timeout=timeout) as response:
        return response.read()

def hyperhdr_json_request(
    host: str,
    port: int,
    payload: dict,
    timeout: float = 5.0,
    log_reply: bool = False,
) -> dict:
    encoded = urllib.parse.quote(json.dumps(payload, separators=(",", ":")))
    url = f"http://{host}:{port}/json-rpc?request={encoded}"

    with urllib.request.urlopen(url, timeout=timeout) as response:
        body = response.read().decode("utf-8", errors="replace")

    if log_reply and DEBUG:
        log(f"HyperHDR reply: {body}")

    data = json.loads(body)
    if isinstance(data, dict) and data.get("success") is False:
        error = data.get("error") or data.get("info") or body
        raise RuntimeError(f"HyperHDR request failed: {error}")

    return data

def build_basic_auth_headers(username: str, password: str) -> dict[str, str]:
    if not username and not password:
        return {}

    token = base64.b64encode(f"{username}:{password}".encode("utf-8")).decode("ascii")
    return {"Authorization": f"Basic {token}"}

def kodi_json_rpc(host: str, port: int, method: str, params: dict, timeout: float = KODI_TIMEOUT) -> dict:
    payload = {
        "jsonrpc": "2.0",
        "id": 1,
        "method": method,
        "params": params,
    }
    encoded = urllib.parse.quote(json.dumps(payload, separators=(",", ":")))
    url = f"http://{host}:{port}/jsonrpc?request={encoded}"
    return http_get_json(url, timeout=timeout, headers=build_basic_auth_headers(KODI_USERNAME, KODI_PASSWORD))

def parse_numeric(value: Optional[str]) -> Optional[float]:
    if value is None:
        return None

    match = NUMERIC_PATTERN.search(value)
    if not match:
        return None

    try:
        return float(match.group(0))
    except ValueError:
        return None

def parse_kodi_dv_fll(raw_labels: dict[str, str]) -> tuple[Optional[float], Optional[float], Optional[float], str]:
    l1_min = parse_numeric(raw_labels.get("Player.Process(video.dovi.l1.min.nits)"))
    l1_max = parse_numeric(raw_labels.get("Player.Process(video.dovi.l1.max.nits)"))
    l1_avg = parse_numeric(raw_labels.get("Player.Process(video.dovi.l1.avg.nits)"))
    if l1_min is not None or l1_max is not None or l1_avg is not None:
        return l1_min, l1_max, l1_avg, "derived-from-l1"

    return None, None, None, "unavailable"


def parse_kodi_dv_rpu_mdl(raw_labels: dict[str, str]) -> tuple[Optional[float], str]:
    label = "Player.Process(video.dovi.source.max.nits)"
    value = parse_numeric(raw_labels.get(label))
    if value is not None and value > 0:
        return value, label

    return None, "unavailable"

def collect_kodi_dv_state(host: str, port: int) -> dict:
    response = kodi_json_rpc(host, port, "XBMC.GetInfoLabels", {"labels": KODI_INFO_LABELS}, timeout=KODI_TIMEOUT)
    result = response.get("result") or {}
    raw_labels = {label: str(result.get(label, "") or "") for label in KODI_INFO_LABELS}

    l1_min = parse_numeric(raw_labels.get("Player.Process(video.dovi.l1.min.nits)"))
    l1_max = parse_numeric(raw_labels.get("Player.Process(video.dovi.l1.max.nits)"))
    l1_avg = parse_numeric(raw_labels.get("Player.Process(video.dovi.l1.avg.nits)"))
    source_min = parse_numeric(raw_labels.get("Player.Process(video.dovi.source.min.nits)"))
    source_max = parse_numeric(raw_labels.get("Player.Process(video.dovi.source.max.nits)"))
    l6_min_lum = parse_numeric(raw_labels.get("Player.Process(video.dovi.l6.min.lum)"))
    l6_max_lum = parse_numeric(raw_labels.get("Player.Process(video.dovi.l6.max.lum)"))
    l6_max_cll = parse_numeric(raw_labels.get("Player.Process(video.dovi.l6.max.cll)"))
    l6_max_fall = parse_numeric(raw_labels.get("Player.Process(video.dovi.l6.max.fall)"))
    rpu_mdl, rpu_mdl_source = parse_kodi_dv_rpu_mdl(raw_labels)
    fll_lower, fll_upper, fll_avg, fll_source = parse_kodi_dv_fll(raw_labels)

    state: dict[str, object] = {
        "available": any(
            value is not None
            for value in (source_min, source_max, l1_min, l1_max, l1_avg, l6_min_lum, l6_max_lum, l6_max_cll, l6_max_fall, rpu_mdl, fll_lower, fll_upper, fll_avg)
        ),
        "host": host,
        "port": port,
        "transport": "XBMC.GetInfoLabels",
        "fllSource": fll_source,
        "rpuMdlSource": rpu_mdl_source,
        "rawLabels": raw_labels,
    }

    if l1_min is not None:
        state["l1MinNits"] = l1_min
    if l1_max is not None:
        state["l1MaxNits"] = l1_max
    if l1_avg is not None:
        state["l1AvgNits"] = l1_avg
    if source_min is not None:
        state["sourceMinNits"] = source_min
    if source_max is not None:
        state["sourceMaxNits"] = source_max
    if l6_min_lum is not None:
        state["l6MinLumNits"] = l6_min_lum
    if l6_max_lum is not None:
        state["l6MaxLumNits"] = l6_max_lum
    if l6_max_cll is not None:
        state["l6MaxCllNits"] = l6_max_cll
    if l6_max_fall is not None:
        state["l6MaxFallNits"] = l6_max_fall
    if rpu_mdl is not None:
        state["rpuMdlNits"] = rpu_mdl
    if fll_lower is not None:
        state["fllLowerCdM2"] = fll_lower
    if fll_upper is not None:
        state["fllUpperCdM2"] = fll_upper
    if fll_avg is not None:
        state["fllAvgCdM2"] = fll_avg

    return state

def vrroom_query(host: str, port: int, command: str, timeout: float = 3.0) -> str:
    with socket.create_connection((host, port), timeout=timeout) as sock:
        sock.settimeout(timeout)
        payload = (command.strip() + "\r\n").encode("utf-8")
        sock.sendall(payload)

        chunks = []
        start = time.time()

        while True:
            try:
                data = sock.recv(4096)
                if not data:
                    break
                chunks.append(data)
                if len(data) < 4096:
                    break
            except socket.timeout:
                break

            if time.time() - start > timeout:
                break

    return b"".join(chunks).decode("utf-8", errors="replace").strip()

def normalize_status_text(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip().lower()

def classify_video_mode(status_text: str) -> Optional[str]:
    """
    Returns one of:
      "lldv"
      "hdr"
      "sdr"
      None  -> could not confidently classify
    """
    s = normalize_status_text(status_text)

    if "no signal" in s:
        return None

    # LLDV first so it doesn't get swallowed by generic HDR matches.
    if re.search(r"\blldv\b", s, re.IGNORECASE):
        return "lldv"

    if re.search(r"\bdolby\s*vision\b", s, re.IGNORECASE):
        return "lldv"

    hdr_patterns = [
        r"\bhdr10\+\b",
        r"\bhdr10\b",
        r"\bhdr\b",
        r"\bhlg\b",
        r"\bsbtm\b",
    ]
    for pattern in hdr_patterns:
        if re.search(pattern, s, re.IGNORECASE):
            return "hdr"

    if re.search(r"\bsdr\b", s, re.IGNORECASE):
        return "sdr"

    if re.search(r"\bbt709\b", s, re.IGNORECASE):
        return "sdr"

    if re.search(r"\bbt2020\b", s, re.IGNORECASE):
        return None

    return None

def parse_avi_infoframe(avi_hex: str) -> dict:
    tokens = [token.strip().lower() for token in (avi_hex or "").split(":") if token.strip()]
    if len(tokens) < 7:
        return {}

    try:
        byte4 = int(tokens[4], 16)
        byte5 = int(tokens[5], 16)
        byte6 = int(tokens[6], 16)
    except ValueError:
        return {}

    colorspace = {
        0x00: "RGB",
        0x20: "YCbCr 422",
        0x40: "YCbCr 444",
        0x60: "YCbCr 420",
    }.get(byte4 & 0xE0, "unknown")

    colorimetry = "unspecified"
    color_bits = byte5 & 0xC0
    if color_bits == 0x40:
        colorimetry = "BT601"
    elif color_bits == 0x80:
        colorimetry = "BT709"
    elif color_bits == 0xC0:
        colorimetry = {
            0x00: "xvYCC601",
            0x10: "xvYCC709",
            0x20: "sYCC601",
            0x30: "AdobeYcc601",
            0x40: "AdobeRGB",
            0x50: "BT2020 YCC",
            0x60: "BT2020 RGB/YCC",
        }.get(byte6 & 0x70, "extended")

    quantization = {
        0x00: "default",
        0x04: "limited",
        0x08: "full",
    }.get(byte6 & 0x0C, "unknown")

    return {
        "colorspace": colorspace,
        "colorimetry": colorimetry,
        "quantization": quantization,
    }

def parse_hdr_infoframe(hdr_hex: str) -> dict:
    tokens = [token.strip().lower() for token in (hdr_hex or "").split(":") if token.strip()]
    if len(tokens) < 30:
        return {}
    if tokens[0] != "87" or tokens[1] != "01":
        return {}

    try:
        eotf_code = int(tokens[4], 16)
        max_display_lum = (int(tokens[23], 16) << 8) | int(tokens[22], 16)
        min_display_lum = (((int(tokens[25], 16) << 8) | int(tokens[24], 16)) * 0.0001)
        max_cll = (int(tokens[27], 16) << 8) | int(tokens[26], 16)
        max_fall = (int(tokens[29], 16) << 8) | int(tokens[28], 16)
    except ValueError:
        return {}

    eotf = {
        0: "sdr",
        1: "hdr",
        2: "pq",
        3: "hlg",
    }.get(eotf_code, f"unknown({eotf_code})")

    return {
        "eotf": eotf,
        "max_display_lum_nits": max_display_lum,
        "min_display_lum_nits": round(min_display_lum, 4),
        "max_cll_nits": max_cll,
        "max_fall_nits": max_fall,
    }

def parse_dv_status(hdr_page: dict) -> dict:
    vsi = [token.strip().lower() for token in str(hdr_page.get("VSI", "")).split(":") if token.strip()]
    hvs = [token.strip().lower() for token in str(hdr_page.get("HVS", "")).split(":") if token.strip()]
    result = {
        "active": False,
        "kind": "none",
        "interface": "none",
    }

    if len(vsi) >= 10 and vsi[0] == "81" and vsi[4:7] == ["46", "d0", "00"]:
        result["active"] = True
        result["kind"] = "lldv"
        try:
            flags = int(vsi[7], 16)
            result["interface"] = "low-latency" if (flags & 0x01) else "standard"
            result["vsi_active_flag"] = bool(flags & 0x02)
            if len(vsi) > 9:
                eff_tmax = ((int(vsi[8], 16) & 0x0F) << 4) | int(vsi[9], 16)
                if eff_tmax > 0:
                    result["backlight_max_luma_code"] = eff_tmax
        except ValueError:
            pass
        return result

    if len(hvs) >= 5 and hvs[0] == "81" and hvs[2] == "18" and hvs[4] == "03":
        result["active"] = True
        result["kind"] = "dv"
        result["interface"] = "standard"
        return result

    return result

def _calc_pq_eotf(normalized_luminance: float) -> float:
    if normalized_luminance <= 0:
        return 0.0

    m1 = 2610.0 / 16384.0
    m2 = 2523.0 / 32.0
    c1 = 3424.0 / 4096.0
    c2 = 2413.0 / 128.0
    c3 = 2392.0 / 128.0

    power = normalized_luminance ** (1.0 / m2)
    numerator = max(power - c1, 0.0)
    denominator = c2 - c3 * power
    if denominator <= 0:
        return 0.0

    luminance = 10000.0 * ((numerator / denominator) ** (1.0 / m1))
    return round(luminance, 3)

def _decode_dv_primaries_v2(tokens: list[int]) -> dict:
    red_x = 0xA0 | (tokens[10] >> 3)
    red_y = 0x40 | (tokens[11] >> 3)
    green_x = tokens[8] >> 1
    green_y = 0x80 | (tokens[9] >> 1)
    blue_x = 0x20 | (tokens[10] & 0x07)
    blue_y = 0x08 | (tokens[11] & 0x07)

    return {
        "red": {"x": round(red_x / 256.0, 3), "y": round(red_y / 256.0, 3)},
        "green": {"x": round(green_x / 256.0, 3), "y": round(green_y / 256.0, 3)},
        "blue": {"x": round(blue_x / 256.0, 3), "y": round(blue_y / 256.0, 3)},
    }

def _decode_dv_primaries_v1_short(tokens: list[int]) -> dict:
    red_x = 0xA0 | (tokens[11] >> 3)
    red_y = 0x40 | (((tokens[11] & 0x07) << 2) | ((tokens[10] & 0x01) << 1) | (tokens[9] & 0x01))
    green_x = tokens[9] >> 1
    green_y = 0x80 | (tokens[10] >> 1)
    blue_x = 0x20 | (tokens[8] >> 5)
    blue_y = 0x08 | ((tokens[8] >> 2) & 0x07)

    return {
        "red": {"x": round(red_x / 256.0, 3), "y": round(red_y / 256.0, 3)},
        "green": {"x": round(green_x / 256.0, 3), "y": round(green_y / 256.0, 3)},
        "blue": {"x": round(blue_x / 256.0, 3), "y": round(blue_y / 256.0, 3)},
    }

def _decode_dv_primaries_v1_long(tokens: list[int]) -> dict:
    return {
        "red": {"x": round(tokens[9] / 256.0, 3), "y": round(tokens[10] / 256.0, 3)},
        "green": {"x": round(tokens[11] / 256.0, 3), "y": round(tokens[12] / 256.0, 3)},
        "blue": {"x": round(tokens[13] / 256.0, 3), "y": round(tokens[14] / 256.0, 3)},
    }

def _extract_dolby_vision_block(edid_bytes: bytes) -> Optional[str]:
    if len(edid_bytes) < 256:
        return None

    extension_count = min(edid_bytes[126], max((len(edid_bytes) // 128) - 1, 0))
    for extension_index in range(extension_count):
        start = 128 * (extension_index + 1)
        block = edid_bytes[start:start + 128]
        if len(block) < 128 or block[0] != 0x02:
            continue

        dtd_offset = block[2] if block[2] else 127
        pos = 4
        while pos < dtd_offset:
            header = block[pos]
            length = header & 0x1F
            if length == 0:
                pos += 1
                continue

            payload = block[pos + 1:pos + 1 + length]
            if ((header >> 5) & 0x07) == 0x07 and len(payload) >= 11 and payload[0] == 0x01 and payload[1:4] == bytes((0x46, 0xD0, 0x00)):
                block_bytes = bytes([header]) + payload
                return ":".join(f"{value:02x}" for value in block_bytes)

            pos += 1 + length

    return None

def parse_dv_sink_capabilities(edid_bytes: bytes, raw_block: str) -> dict:
    block = (raw_block or "").strip().lower()
    if not block:
        block = _extract_dolby_vision_block(edid_bytes) or ""

    tokens = []
    for token in block.split(":"):
        token = token.strip().lower()
        if not token:
            continue
        try:
            tokens.append(int(token, 16))
        except ValueError:
            return {}

    if len(tokens) < 12 or (tokens[0] >> 5) != 0x07:
        return {}

    version_code = tokens[5] >> 5
    result = {
        "rawBlock": block,
        "version": version_code,
    }

    if version_code == 0:
        dm_token = tokens[21] if len(tokens) > 21 else 0
        min_disp_lum = ((tokens[19] << 4) | ((tokens[18] >> 4) & 0x0F)) / 4095.0
        max_disp_lum = ((tokens[20] << 4) | (tokens[18] & 0x0F)) / 4095.0
        result["dmVersion"] = f"{dm_token >> 4}.{dm_token & 0x0F}"
        result["interface"] = "standard DV only"
        result["supports2160p60"] = bool(tokens[5] & 0x02)
        result["supportsYuv42212Bit"] = bool(tokens[5] & 0x01)
        result["globalDimming"] = bool(tokens[5] & 0x04)
        result["backlightControl"] = {"supported": False, "defaultNits": 100}
        result["primaries"] = {
            "red": {"x": round((((tokens[7] << 4) | ((tokens[6] >> 4) & 0x0F)) / 4096.0), 3), "y": round((((tokens[8] << 4) | (tokens[6] & 0x0F)) / 4096.0), 3)},
            "green": {"x": round((((tokens[10] << 4) | ((tokens[9] >> 4) & 0x0F)) / 4096.0), 3), "y": round((((tokens[11] << 4) | (tokens[9] & 0x0F)) / 4096.0), 3)},
            "blue": {"x": round((((tokens[13] << 4) | ((tokens[12] >> 4) & 0x0F)) / 4096.0), 3), "y": round((((tokens[14] << 4) | (tokens[13] & 0x0F)) / 4096.0), 3)},
            "white": {"x": round((((tokens[16] << 4) | ((tokens[15] >> 4) & 0x0F)) / 4096.0), 3), "y": round((((tokens[17] << 4) | (tokens[15] & 0x0F)) / 4096.0), 3)},
        }
        result["luminance"] = {
            "maxNits": round(_calc_pq_eotf(max_disp_lum)),
            "minNits": round(_calc_pq_eotf(min_disp_lum), 3),
        }
        return result

    if version_code == 1:
        block_length = tokens[0] & 0x1F
        dm_version_code = (tokens[5] & 0x1C) >> 2
        result["dmVersion"] = {0: "v2.x", 1: "v3.x"}.get(dm_version_code, "reserved")
        result["supports2160p60"] = bool(tokens[5] & 0x02)
        result["supportsYuv42212Bit"] = bool(tokens[5] & 0x01)
        result["globalDimming"] = bool(tokens[6] & 0x01)
        result["backlightControl"] = {"supported": False, "defaultNits": 100}
        result["luminance"] = {
            "maxNits": int((tokens[6] >> 1) * 50 + 100),
            "minNits": round(((tokens[7] >> 1) / 127.0) ** 2, 3),
        }

        if block_length == 0x0B:
            result["interface"] = {
                0: "standard DV only",
                1: "standard and low latency 422 12bit",
                2: "reserved",
                3: "reserved",
            }.get(tokens[8] & 0x03, "reserved")
            result["primaries"] = _decode_dv_primaries_v1_short(tokens)
        else:
            result["interface"] = "standard DV only"
            result["primaries"] = _decode_dv_primaries_v1_long(tokens)

        return result

    if version_code == 2:
        dm_version_code = (tokens[5] & 0x1C) >> 2
        backlight_min_luma = tokens[6] & 0x03
        backlight_supported = bool(tokens[5] & 0x02)
        result["dmVersion"] = {0: "v2.x", 1: "v3.x"}.get(dm_version_code, "reserved")
        result["supports2160p60"] = True
        result["supportsYuv42212Bit"] = bool(tokens[5] & 0x01)
        result["globalDimming"] = bool(tokens[6] & 0x04)
        result["backlightControl"] = {
            "supported": backlight_supported,
            "defaultNits": 25 * (backlight_min_luma + 1) if backlight_supported else 100,
        }
        min_disp_lum = ((tokens[6] >> 3) * 20) / 4095.0
        max_disp_lum = (((tokens[7] >> 3) * 65) + 2055) / 4095.0
        result["luminance"] = {
            "maxNits": round(_calc_pq_eotf(max_disp_lum)),
            "minNits": round(_calc_pq_eotf(min_disp_lum), 3),
        }
        result["primaries"] = _decode_dv_primaries_v2(tokens)
        result["interface"] = {
            0: "low latency 422 12bit",
            1: "low latency 422 12bit and RGB/444 10/12bit",
            2: "standard and low latency 422 12bit",
            3: "standard and low latency 422/444/RGB 10/12bit",
        }.get(tokens[7] & 0x03, "reserved")
        return result

    return {}

def collect_vrroom_info(host: str) -> dict:
    base = f"http://{host}"
    info_page = http_get_json(base + "/ssi/infopage.ssi", timeout=SOCKET_TIMEOUT)
    hdr_page = http_get_json(base + "/ssi/hdrpage.ssi", timeout=SOCKET_TIMEOUT)
    edid_tx0 = http_get_bytes(base + "/ssi/edidtx0.ssi", timeout=SOCKET_TIMEOUT)
    edid_tx1 = http_get_bytes(base + "/ssi/edidtx1.ssi", timeout=SOCKET_TIMEOUT)

    avi = parse_avi_infoframe(str(hdr_page.get("AVI", "")))
    hdr = parse_hdr_infoframe(str(hdr_page.get("HDR", "")))
    dv = parse_dv_status(hdr_page)
    dv_sink_tx0 = parse_dv_sink_capabilities(edid_tx0, str(hdr_page.get("DV0", "") or "").strip())
    dv_sink_tx1 = parse_dv_sink_capabilities(edid_tx1, str(hdr_page.get("DV1", "") or "").strip())

    return {
        "portseltx0": info_page.get("portseltx0", "").strip(),
        "portseltx1": info_page.get("portseltx1", "").strip(),
        "source_name": info_page.get("SPDASC", "").strip(),
        "source_name1": info_page.get("SPDASC1", "").strip(),
        "rx0": info_page.get("RX0", "").strip(),
        "rx1": info_page.get("RX1", "").strip(),
        "rx2": info_page.get("RX2", "").strip(),
        "rx3": info_page.get("RX3", "").strip(),
        "tx0": info_page.get("TX0", "").strip(),
        "tx1": info_page.get("TX1", "").strip(),
        "sink0": info_page.get("SINK0", "").strip(),
        "sink1": info_page.get("SINK1", "").strip(),
        "avi": avi,
        "hdr": hdr,
        "dv": dv,
        "dv_sink_tx0": dv_sink_tx0,
        "dv_sink_tx1": dv_sink_tx1,
    }

def get_status_target_source_name(info: Optional[dict], status_target: str) -> str:
    if not info:
        return ""

    source_name_tx0 = str(info.get("source_name", "") or "").strip()
    source_name_tx1 = str(info.get("source_name1", "") or "").strip()

    if status_target == "tx1":
        return source_name_tx1 or source_name_tx0

    return source_name_tx0 or source_name_tx1

def iter_input_name_candidates(input_name: str):
    base_name = INPUT_DEVICE_NAMES.get(input_name)
    if base_name:
        yield base_name

    for override_name in INPUT_DEVICE_OVERRIDE_NAMES.get(input_name, {}).values():
        yield override_name

def match_input_from_text(text: str) -> Optional[str]:
    normalized = normalize_status_text(text)
    if not normalized:
        return None

    direct_match = re.search(r"\brx([0-3])\b", normalized, re.IGNORECASE)
    if direct_match:
        return f"rx{direct_match.group(1)}"

    for input_name in ("rx0", "rx1", "rx2", "rx3"):
        for candidate in iter_input_name_candidates(input_name):
            candidate_text = normalize_status_text(candidate)
            if candidate_text and candidate_text in normalized:
                return input_name

    return None

def parse_selected_route_target(raw_value: object) -> Optional[str]:
    text = normalize_status_text(str(raw_value or ""))
    if not text:
        return None

    if text in {"0", "1", "2", "3"}:
        return f"rx{text}"

    direct_input = match_input_from_text(text)
    if direct_input:
        return direct_input

    if text == "4" or re.search(r"\btx0\b", text, re.IGNORECASE):
        return "tx0"

    if text == "5" or re.search(r"\btx1\b", text, re.IGNORECASE):
        return "tx1"

    return None

def build_transfer_info_summary(mode: Optional[str], info: dict) -> str:
    avi = info.get("avi", {})
    hdr = info.get("hdr", {})
    dv = info.get("dv", {})

    parts = [
        f"mode={mode or 'unknown'}",
        f"source={get_status_target_source_name(info, STATUS_TARGET) or 'unknown'}",
    ]

    active_input = detect_active_input(info, STATUS_TARGET)
    if active_input:
        device_name = resolve_device_name_for_input(active_input) or active_input
        parts.append(f"input={active_input}/{device_name}")
    portseltx0 = str(info.get("portseltx0", "") or "").strip()
    portseltx1 = str(info.get("portseltx1", "") or "").strip()
    if portseltx0 or portseltx1:
        parts.append(f"insel=tx0:{portseltx0 or '?'} tx1:{portseltx1 or '?'}")

    tx1 = info.get("tx1") or ""
    if tx1:
        parts.append(f"tx1={tx1}")

    if avi:
        parts.append(
            "avi=" + "/".join(
                filter(
                    None,
                    [
                        avi.get("colorspace", ""),
                        avi.get("colorimetry", ""),
                        avi.get("quantization", ""),
                    ],
                )
            )
        )

    if hdr:
        parts.append(f"eotf={hdr.get('eotf', 'unknown')}")
        if hdr.get("max_cll_nits"):
            parts.append(f"maxcll={hdr['max_cll_nits']}")
        if hdr.get("max_fall_nits"):
            parts.append(f"maxfall={hdr['max_fall_nits']}")
        if hdr.get("max_display_lum_nits"):
            parts.append(f"masteringMax={hdr['max_display_lum_nits']}")

    if dv.get("active"):
        parts.append(f"dv={dv.get('kind', 'active')}")
        if dv.get("interface") and dv.get("interface") != "none":
            parts.append(f"dv_if={dv['interface']}")

    return "VRROOM transfer info: " + ", ".join(parts)

def parse_status_signal_details(status_text: str) -> dict:
    details: dict[str, object] = {}
    normalized = re.sub(r"\s+", " ", (status_text or "").strip())

    chroma_match = re.search(r"\b(444|422|420)\b", normalized, re.IGNORECASE)
    if chroma_match:
        details["chroma_subsampling"] = chroma_match.group(1)

    bit_depth_match = re.search(r"\b(8|10|12|16)b\b", normalized, re.IGNORECASE)
    if bit_depth_match:
        details["bit_depth"] = int(bit_depth_match.group(1))

    pixel_clock_match = re.search(r"\b(\d+(?:\.\d+)?)\s*mhz\b", normalized, re.IGNORECASE)
    if pixel_clock_match:
        pixel_clock = float(pixel_clock_match.group(1))
        details["pixel_clock_mhz"] = int(pixel_clock) if pixel_clock.is_integer() else pixel_clock

    color_space_match = re.search(r"\b(bt2020|bt709|bt601)\b", normalized, re.IGNORECASE)
    if color_space_match:
        details["colorspace_text"] = color_space_match.group(1).upper()

    if re.search(r"\blldv\b", normalized, re.IGNORECASE):
        details["dynamic_range"] = "lldv"
    elif re.search(r"\bdolby\s*vision\b", normalized, re.IGNORECASE):
        details["dynamic_range"] = "dolby-vision"
    elif re.search(r"\bhlg\b", normalized, re.IGNORECASE):
        details["dynamic_range"] = "hlg"
    elif re.search(r"\bhdr10\+\b", normalized, re.IGNORECASE):
        details["dynamic_range"] = "hdr10+"
    elif re.search(r"\bhdr10\b", normalized, re.IGNORECASE):
        details["dynamic_range"] = "hdr10"
    elif re.search(r"\bhdr\b", normalized, re.IGNORECASE):
        details["dynamic_range"] = "hdr"
    elif re.search(r"\bsdr\b", normalized, re.IGNORECASE):
        details["dynamic_range"] = "sdr"

    return details


def normalize_report_dynamic_range(mode: Optional[str], signal: dict[str, object]) -> Optional[str]:
    explicit = str(signal.get("dynamic_range", "") or "").strip().lower()
    if explicit:
        return explicit

    normalized_mode = str(mode or "").strip().lower()
    if normalized_mode == "lldv":
        return "lldv"
    if normalized_mode == "hdr":
        return "hdr"
    if normalized_mode == "sdr":
        return "sdr"

    return None

def build_video_stream_report_state(
    status_text: str,
    mode: Optional[str],
    info: Optional[dict],
    active_input: Optional[str],
) -> dict:
    signal = parse_status_signal_details(status_text)
    normalized_dynamic_range = normalize_report_dynamic_range(mode, signal)
    if normalized_dynamic_range:
        signal["dynamic_range"] = normalized_dynamic_range

    report: dict[str, object] = {
        "reportedBy": "vrroom-sync",
        "statusTarget": STATUS_TARGET,
        "rawStatus": (status_text or "").strip(),
        "classifiedMode": mode or "",
        "signal": signal,
    }

    if active_input:
        report["activeInput"] = active_input
        device_name = resolve_device_name_for_input(active_input)
        if device_name:
            report["activeDevice"] = device_name

    info = info or {}
    target_source_name = get_status_target_source_name(info, STATUS_TARGET)
    if target_source_name:
        report["sourceName"] = target_source_name

    if info.get("source_name"):
        report["sourceNameTx0"] = str(info.get("source_name") or "")

    if info.get("source_name1"):
        report["sourceNameTx1"] = str(info.get("source_name1") or "")

    route = {
        "portseltx0": str(info.get("portseltx0") or ""),
        "portseltx1": str(info.get("portseltx1") or ""),
    }
    if route["portseltx0"] or route["portseltx1"]:
        report["route"] = route

    outputs = {
        "tx0": str(info.get("tx0") or ""),
        "tx1": str(info.get("tx1") or ""),
    }
    if outputs["tx0"] or outputs["tx1"]:
        report["outputs"] = outputs

    inputs = {
        key: str(info.get(key) or "")
        for key in ("rx0", "rx1", "rx2", "rx3")
        if str(info.get(key) or "")
    }
    if inputs:
        report["inputs"] = inputs

    avi = info.get("avi") or {}
    if avi:
        report["avi"] = dict(avi)
    else:
        report["avi"] = None

    hdr = info.get("hdr") or {}
    if mode == "hdr" and hdr:
        report["hdr"] = dict(hdr)
    else:
        report["hdr"] = None

    dv = info.get("dv") or {}
    if mode == "lldv" and dv:
        report["dv"] = dict(dv)
    else:
        report["dv"] = None

    dv_sink_tx0 = info.get("dv_sink_tx0") or {}
    if mode == "lldv" and dv_sink_tx0:
        report["dvSinkTx0"] = dict(dv_sink_tx0)
    else:
        report["dvSinkTx0"] = None

    dv_sink_tx1 = info.get("dv_sink_tx1") or {}
    if mode == "lldv" and dv_sink_tx1:
        report["dvSinkTx1"] = dict(dv_sink_tx1)
    else:
        report["dvSinkTx1"] = None

    return report

def should_poll_kodi_dv(mode: Optional[str], info: Optional[dict]) -> bool:
    if not KODI_HOST.strip():
        return False

    if mode == "lldv":
        return True

    dv = (info or {}).get("dv") or {}
    return dv.get("active") is True

def try_collect_kodi_dv_state(last_error: Optional[str]) -> tuple[dict, Optional[str]]:
    try:
        kodi_dv_state = collect_kodi_dv_state(KODI_HOST, KODI_PORT)
        if last_error is not None and DEBUG:
            log("Kodi DV metadata polling recovered.")
        return kodi_dv_state, None
    except Exception as exc:
        kodi_dv_state = {
            "available": False,
            "host": KODI_HOST,
            "port": KODI_PORT,
            "transport": "XBMC.GetInfoLabels",
            "error": str(exc),
        }
        if str(exc) != last_error:
            log(f"Warning: could not collect Kodi DV metadata: {exc}")
        return kodi_dv_state, str(exc)

def desired_transfer_profile_for_source(mode: Optional[str], status_text: str, info: Optional[dict]) -> Optional[str]:
    if mode is None:
        return None

    normalized_status = normalize_status_text(status_text)
    hdr = (info or {}).get("hdr", {})
    dv = (info or {}).get("dv", {})
    eotf = str(hdr.get("eotf", "")).strip().lower()

    if dv.get("active") or mode == "lldv":
        return TRANSFER_PROFILE_PQ

    if eotf == "hlg" or re.search(r"\bhlg\b", normalized_status, re.IGNORECASE):
        return TRANSFER_PROFILE_HLG

    if mode == "sdr" or eotf == "sdr":
        return TRANSFER_PROFILE_SDR

    return TRANSFER_PROFILE_PQ

def detect_active_input(info: Optional[dict], status_target: str) -> Optional[str]:
    if not info:
        return None

    target_text = normalize_status_text(str(info.get(status_target, "") or ""))
    target_source_name = normalize_status_text(get_status_target_source_name(info, status_target))
    source_name_tx0 = normalize_status_text(str(info.get("source_name", "") or ""))
    source_name_tx1 = normalize_status_text(str(info.get("source_name1", "") or ""))
    rx_keys = ["rx0", "rx1", "rx2", "rx3"]

    portseltx0 = str(info.get("portseltx0", "") or "").strip()
    portseltx1 = str(info.get("portseltx1", "") or "").strip()

    if status_target == "tx0":
        selected = parse_selected_route_target(portseltx0)
        if selected and selected.startswith("rx"):
            return selected

    if status_target == "tx1":
        selected = parse_selected_route_target(portseltx1)
        if selected == "tx0":
            upstream = parse_selected_route_target(portseltx0)
            if upstream and upstream.startswith("rx"):
                return upstream
        elif selected and selected.startswith("rx"):
            return selected

    for candidate_text in (target_text, target_source_name, source_name_tx1, source_name_tx0):
        matched_input = match_input_from_text(candidate_text)
        if matched_input:
            return matched_input

    for rx_key in rx_keys:
        rx_text = normalize_status_text(str(info.get(rx_key, "") or ""))
        if not rx_text:
            continue
        for candidate_text in (target_text, target_source_name, source_name_tx1, source_name_tx0):
            if not candidate_text:
                continue
            if rx_text in candidate_text or candidate_text in rx_text:
                return rx_key

    return None

def hyperhdr_set_hdr(host: str, port: int, enabled: bool) -> None:
    payload = {
        "command": "videomodehdr",
        "HDR": 1 if enabled else 0,
    }
    hyperhdr_json_request(host, port, payload, timeout=5.0, log_reply=True)

def hyperhdr_get_hdr_state(host: str, port: int) -> Optional[bool]:
    try:
        data = hyperhdr_json_request(host, port, {"command": "serverinfo"}, timeout=5.0)
    except Exception as exc:
        log(f"Warning: could not query HyperHDR state: {exc}")
        return None

    value = find_key(data, "videomodehdr")
    if value is None:
        return None

    if isinstance(value, bool):
        return value
    if isinstance(value, int):
        return value != 0
    if isinstance(value, str):
        return value.strip().lower() not in ("0", "false", "")

    return None

def hyperhdr_report_video_stream(host: str, port: int, state: dict, instance: int = 0) -> dict:
    payload = {
        "command": "current-state",
        "subcommand": "video-stream-report",
        "instance": instance,
        "state": state,
    }
    return hyperhdr_json_request(host, port, payload, timeout=5.0, log_reply=False)

def desired_hdr_state_for_mode(mode: str) -> bool:
    return mode in ("hdr", "lldv")

def ensure_lut_files_exist() -> None:
    required = [HDR_LUT, LLDV_LUT]
    missing = [str(p) for p in required if not p.exists()]
    if missing:
        raise FileNotFoundError(f"Missing LUT file(s): {', '.join(missing)}")

def lut_signature(path: Path) -> Optional[int]:
    try:
        return path.stat().st_mtime_ns
    except FileNotFoundError:
        return None

# =========================
# Main loop
# =========================

def main() -> int:
    ensure_lut_files_exist()
    load_input_override_state()
    _override_server = start_input_override_server()

    known_hyperhdr_state = hyperhdr_get_hdr_state(HYPERHDR_HOST, HYPERHDR_PORT)
    last_classification: Optional[str] = None
    stable_count = 0
    last_info_summary: Optional[str] = None
    last_kodi_dv_error: Optional[str] = None
    last_reported_video_stream: Optional[dict] = None
    last_reported_kodi_dv_state: Optional[dict] = None
    last_raw_status: str = ""
    last_mode: Optional[str] = None
    last_transfer_info: Optional[dict] = None
    last_active_input: Optional[str] = None
    last_video_stream_report: Optional[dict] = None
    info_poll_counter = 0
    next_vrroom_poll_at = 0.0
    next_kodi_dv_poll_at = 0.0

    if DEBUG:
        log(f"Initial HyperHDR HDR state: {known_hyperhdr_state}")

    while True:
        try:
            loop_now = time.monotonic()
            did_vrroom_poll = False
            kodi_dv_state: Optional[dict] = None

            if loop_now >= next_vrroom_poll_at:
                did_vrroom_poll = True
                next_vrroom_poll_at = loop_now + VRROOM_POLL_SECONDS

                raw = vrroom_query(
                    VRROOM_HOST,
                    VRROOM_PORT,
                    f"get status {STATUS_TARGET}",
                    timeout=SOCKET_TIMEOUT,
                )
                last_raw_status = raw

                if DEBUG:
                    log(f"VRROOM raw [{STATUS_TARGET}]: {raw}")

                last_mode = classify_video_mode(raw)
                last_transfer_info = None
                last_active_input = None

                info_poll_counter += 1
                try:
                    last_transfer_info = collect_vrroom_info(VRROOM_HOST)
                    last_active_input = detect_active_input(last_transfer_info, STATUS_TARGET)
                    info_summary = build_transfer_info_summary(last_mode, last_transfer_info)
                    if info_summary != last_info_summary or info_poll_counter >= INFO_LOG_INTERVAL_POLLS:
                        log(info_summary)
                        last_info_summary = info_summary
                        info_poll_counter = 0
                except Exception as exc:
                    if DEBUG:
                        log(f"Warning: could not collect VRROOM transfer info: {exc}")

                last_video_stream_report = build_video_stream_report_state(last_raw_status, last_mode, last_transfer_info, last_active_input)

            mode = last_mode
            transfer_info = last_transfer_info
            active_input = last_active_input
            video_stream_report = last_video_stream_report or {}
            poll_kodi_dv = should_poll_kodi_dv(mode, transfer_info)
            should_poll_kodi_now = poll_kodi_dv and (did_vrroom_poll or last_reported_kodi_dv_state is None or loop_now >= next_kodi_dv_poll_at)

            if should_poll_kodi_now:
                kodi_dv_state, last_kodi_dv_error = try_collect_kodi_dv_state(last_kodi_dv_error)
                next_kodi_dv_poll_at = loop_now + KODI_DV_POLL_SECONDS
            elif last_kodi_dv_error is not None and not poll_kodi_dv:
                last_kodi_dv_error = None

            if did_vrroom_poll and video_stream_report != last_reported_video_stream:
                try:
                    report_payload = dict(video_stream_report)
                    if poll_kodi_dv:
                        current_kodi_state = dict(kodi_dv_state or last_reported_kodi_dv_state or {})
                        report_payload["kodiDv"] = current_kodi_state
                        last_reported_kodi_dv_state = current_kodi_state
                    elif last_reported_kodi_dv_state is not None:
                        report_payload["kodiDv"] = None
                        last_reported_kodi_dv_state = None

                    hyperhdr_report_video_stream(HYPERHDR_HOST, HYPERHDR_PORT, report_payload)
                    last_reported_video_stream = video_stream_report
                    if DEBUG:
                        log("Reported video stream metadata to HyperHDR.")
                except Exception as exc:
                    log(f"Warning: could not report video stream metadata to HyperHDR: {exc}")
            elif should_poll_kodi_now and poll_kodi_dv and kodi_dv_state != last_reported_kodi_dv_state:
                try:
                    hyperhdr_report_video_stream(HYPERHDR_HOST, HYPERHDR_PORT, {"kodiDv": dict(kodi_dv_state or {})})
                    last_reported_kodi_dv_state = dict(kodi_dv_state or {})
                    if DEBUG:
                        log("Reported Kodi DV runtime metadata delta to HyperHDR.")
                except Exception as exc:
                    log(f"Warning: could not report Kodi DV metadata delta to HyperHDR: {exc}")
            elif not poll_kodi_dv and last_reported_kodi_dv_state is not None:
                try:
                    hyperhdr_report_video_stream(HYPERHDR_HOST, HYPERHDR_PORT, {"kodiDv": None})
                    last_reported_kodi_dv_state = None
                    next_kodi_dv_poll_at = loop_now
                    if DEBUG:
                        log("Cleared Kodi DV runtime metadata in HyperHDR.")
                except Exception as exc:
                    log(f"Warning: could not clear Kodi DV metadata in HyperHDR: {exc}")

            if not did_vrroom_poll:
                time.sleep(LOOP_SLEEP_SECONDS)
                continue

            if mode is None:
                if DEBUG:
                    log("Could not confidently classify status; leaving HyperHDR/LUT unchanged.")
                last_classification = None
                stable_count = 0
                time.sleep(LOOP_SLEEP_SECONDS)
                continue

            if mode == last_classification:
                stable_count += 1
            else:
                last_classification = mode
                stable_count = 1

            if DEBUG:
                log(f"Classified as {mode.upper()} ({stable_count}/{DEBOUNCE_POLLS} stable polls)")

            if stable_count < DEBOUNCE_POLLS:
                time.sleep(LOOP_SLEEP_SECONDS)
                continue

            chroma_subsampling = (video_stream_report.get("signal") or {}).get("chroma_subsampling")
            desired_hdr = desired_hdr_state_for_mode(mode)

            if DEBUG:
                device_name = resolve_device_name_for_input(active_input) or active_input or "unknown"
                log(
                    f"Stable video mode={mode} input={active_input or 'unknown'}/{device_name} chroma={chroma_subsampling or 'unknown'}; HyperHDR will evaluate runtime LUT bucket selection."
                )

            if known_hyperhdr_state != desired_hdr:
                log(f"Setting HyperHDR HDR tone mapping to {'ON' if desired_hdr else 'OFF'}")
                hyperhdr_set_hdr(HYPERHDR_HOST, HYPERHDR_PORT, desired_hdr)
                known_hyperhdr_state = desired_hdr
            else:
                if DEBUG:
                    log(f"HyperHDR already {'ON' if desired_hdr else 'OFF'}; no change needed.")

        except KeyboardInterrupt:
            log("Exiting.")
            return 0
        except Exception as exc:
            log(f"Loop error: {exc}")

        time.sleep(LOOP_SLEEP_SECONDS)

if __name__ == "__main__":
    sys.exit(main())
