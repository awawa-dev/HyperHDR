![C++20](https://img.shields.io/badge/Language-C%2B%2B20-blue) ![CMake](https://img.shields.io/badge/Build%20System-CMake-orange) ![MIT](https://img.shields.io/badge/License-MIT-brightgreen) ![Objective‑C++](https://img.shields.io/badge/Language-Objective‑C%2B%2B-lightblue) ![JavaScript](https://img.shields.io/badge/Language-JavaScript-teal) ![HTML](https://img.shields.io/badge/Language-HTML-orange) ![Linear Algebra](https://img.shields.io/badge/Linear%20Algebra-Vector%20Computations-maroon) ![JSON API](https://img.shields.io/badge/JSON%20API-supported-blue) ![Bootstrap 5](https://img.shields.io/badge/UI‑Framework-Bootstrap%205-purple) ![DirectX](https://img.shields.io/badge/DirectX-supported-lightblue) ![Wayland‑PipeWire](https://img.shields.io/badge/Wayland‑PipeWire-supported-teal) ![Sound Reactive](https://img.shields.io/badge/Sound-Reactive-brightgreen) ![MQTT](https://img.shields.io/badge/MQTT-supported-orange) ![mDNS‑Zeroconf](https://img.shields.io/badge/mDNS‑Zeroconf-supported-lightblue) ![USB Grabbers](https://img.shields.io/badge/USB%20Grabbers-enabled-blueviolet) ![Raspberry Pi ARM](https://img.shields.io/badge/Raspberry%20Pi‑ARM-64bit-brightgreen) ![x64](https://img.shields.io/badge/Architecture-x64-blue) ![ARM](https://img.shields.io/badge/Architecture-ARM-orange) ![WLED](https://img.shields.io/badge/WLED-supported-brightgreen) ![Home Assistant](https://img.shields.io/badge/Home%20Assistant-supported-orange) ![Adalight](https://img.shields.io/badge/Adalight-supported-purple)

## Fork Disclaimer

> **This is an unofficial fork of [HyperHDR](https://github.com/awawa-dev/HyperHDR).**
> Awawa-dev does not maintain this fork, will not diagnose issues related to this fork, and this fork may drift from the main branch.
> This fork is experimental and may break known-working features. Currently only tested on a Raspberry Pi 5 with 8GB RAM.
>
> Fork repository: https://github.com/JChalka/HyperHDR/tree/BFI
>
> For the official HyperHDR project, documentation, and support, visit:
> - Releases: https://github.com/awawa-dev/HyperHDR/releases
> - Wiki: https://wiki.hyperhdr.eu/
> - Support Forum: https://github.com/awawa-dev/HyperHDR/discussions

---

## About

HyperHDR is an open-source ambient lighting system for TVs and music setups, based on real-time video and audio stream analysis. It is designed with a strong focus on stability, high performance, and high-fidelity video decoding and mapping for precise and vibrant LED visuals.

This fork (`22.0.0beta2-bfi`) extends HyperHDR with custom LED drivers, LUT management features, and calibration tools for a Teensy-based temporal Blended Frame Insertion (BFI) workflow.

---

## Fork-Specific Features

### Custom LED Drivers

**RawHID** -- Linux-only USB HID LED driver for Teensy-based temporal BFI:
- Companion Teensy sketch: [HyperTeensy_Temporal_Blend](https://github.com/JChalka/Blended-Frame-Insertion/tree/main/examples/HyperTeensy_Temporal_Blend)
- Explicit `hidraw` device selection with auto-discovery by VID/PID
- 16-bit RGB and packed 12-bit carry payload modes
- Optional sideband log reads from the device
- Host-owned transfer curve, calibration header, and RGBW pipeline stages
- Scene policy modes: `solverAwareV2` (legacy energy model) and `solverAwareV3` (direct solver emitted-output energy model)
- Full Q16 (16-bit) float pipeline through highlight analysis and output preparation

**Adalight16** -- 16-bit serial transport extending the standard Adalight driver:
- Configurable `writeTimeout` (blocking or non-blocking serial writes)
- Host ICE RGBW output and legacy white-channel calibration trailer support

**RawHID Permissions Note:** The RawHID driver opens `/dev/hidraw*` with read/write access. HyperHDR must have permission to access the device; use appropriate udev rules or group membership for the runtime user. Example udev rule:
```
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="16c0", ATTRS{idProduct}=="0486", MODE="0660", GROUP="plugdev"
```

### Transfer, Calibration & Solver Header Uploads

The LED Hardware web UI supports upload, selection, and management of:
- **Transfer curve headers** -- stored in `~/.hyperhdr/transfer_headers/`
- **Calibration headers** -- stored in `~/.hyperhdr/calibration_headers/`
- **Solver profiles** -- stored in `~/.hyperhdr/solver_profiles/` (temporal runtime solver `.h` headers are the preferred format)
- **RGBW LUT headers** -- for measured TemporalBFI data

These are managed through the `transfer-headers`, `calibration-headers`, and `solver-profiles` JSON-RPC API commands.

### LUT Switching & Signal Detection

- **LUT memory caching** with cache-first fast path -- checks in-memory QHash before any file I/O
- **Memory-scaled LRU cache cap** based on 25% total RAM (or total RAM minus 512MB reserved), floor at 2 entries
- **VRRoom/Kodi signal detection** for automatic LUT selection based on detected content type -- see [`tools/vrroom_hyperhdr_sync.py`](tools/vrroom_hyperhdr_sync.py), a standalone Python bridge that polls a Vertex2/VRRoom HDMI matrix for SDR / HDR10 / HLG / Dolby Vision status and pushes a classified `video-stream-report` to HyperHDR's JSON-RPC API so the runtime can select the correct LUT and toggle HDR tone mapping automatically. Kodi JSON-RPC polling for per-scene DV RPU metadata (L1/L6 nits) is also supported and used by the FLL-based DV runtime LUT bucket model; full Kodi-only signal detection (without VRRoom) is not yet implemented.
- **Dolby Vision runtime switching** support with per-scene metadata awareness (FLL-based bucket model)
- Per-input LUT overrides scoped to `4:2:0` chroma (higher chroma variants deferred pending reliable playback sources)

### RGBW Extraction Methods (Experimental)

Multiple RGBW extraction approaches are available through the Infinite Color Engine:
- **RGBW LUT Header mode** -- the current preferred method when working from measured TemporalBFI data. Note: initial LUT cube loading may take a while once selected.
- Energy-preserving extraction with temperature-aware white point mapping
- Temporal dithering with motion-adaptive diffusion and anti-flicker hysteresis
- Legacy white-channel calibration support for sk6812 drivers

**Highlight policy** has been left in the UI, but dedicated transfer curves are the preferred method of managing highlights and brightness.

### LUT Calibration Changes

- **Color aspect mode selection** added to the calibration UI
- **Dynamic High/Low blend** mode for calibration
- **Configurable Nits override** for HDR mastering metadata
- Calibration override guards in V4L2 grabber paths

### Remote Control

- **Force Grabber Revive** added to the source selection remote control interface

### V4L2 Grabber

- Input 0 dropdown fix: input 0 is now always selectable regardless of input count

---

## Upstream HyperHDR

This fork is based on HyperHDR v22 and periodically merges upstream changes. All upstream features of HyperHDR are preserved, including the Infinite Color Engine, deep-color support, RGBW temporal dithering, PipeWire/DirectX capture, MQTT/Home Assistant integration, and the full LED controller ecosystem.

For the complete upstream feature list and documentation, see the [official HyperHDR README](https://github.com/awawa-dev/HyperHDR).

---

## Building

See [BUILDING.md](BUILDING.md) for compilation instructions.

Quick build on Raspberry Pi:
```bash
./build.sh
```

---

## License

Released under the MIT License
[![GitHub license](https://img.shields.io/badge/License-MIT-yellow.svg)](https://raw.githubusercontent.com/awawa-dev/HyperHDR/master/LICENSE)
