![C++20](https://img.shields.io/badge/Language-C%2B%2B20-blue) ![CMake](https://img.shields.io/badge/Build%20System-CMake-orange) ![MIT](https://img.shields.io/badge/License-MIT-brightgreen) ![Objective‑C++](https://img.shields.io/badge/Language-Objective‑C%2B%2B-lightblue) ![JavaScript](https://img.shields.io/badge/Language-JavaScript-teal) ![HTML](https://img.shields.io/badge/Language-HTML-orange) ![Linear Algebra](https://img.shields.io/badge/Linear%20Algebra-Vector%20Computations-maroon) ![JSON API](https://img.shields.io/badge/JSON%20API-supported-blue) ![Bootstrap 5](https://img.shields.io/badge/UI‑Framework-Bootstrap%205-purple) ![DirectX](https://img.shields.io/badge/DirectX-supported-lightblue) ![Wayland‑PipeWire](https://img.shields.io/badge/Wayland‑PipeWire-supported-teal) ![Sound Reactive](https://img.shields.io/badge/Sound-Reactive-brightgreen) ![MQTT](https://img.shields.io/badge/MQTT-supported-orange) ![mDNS‑Zeroconf](https://img.shields.io/badge/mDNS‑Zeroconf-supported-lightblue) ![USB Grabbers](https://img.shields.io/badge/USB%20Grabbers-enabled-blueviolet) ![Raspberry Pi ARM](https://img.shields.io/badge/Raspberry%20Pi‑ARM-64bit-brightgreen) ![x64](https://img.shields.io/badge/Architecture-x64-blue) ![ARM](https://img.shields.io/badge/Architecture-ARM-orange) ![WLED](https://img.shields.io/badge/WLED-supported-brightgreen) ![Home Assistant](https://img.shields.io/badge/Home%20Assistant-supported-orange) ![Adalight](https://img.shields.io/badge/Adalight-supported-purple)

## About

HyperHDR is an open-source ambient lighting system for TVs and music setups. It performs real-time analysis of video and audio streams to create immersive LED lighting. Designed with a strong focus on stability, high performance, and high-fidelity video decoding and mapping, HyperHDR delivers precise and vibrant LED visuals. Optimized for both single- and multi-threaded video processing, it runs seamlessly on **Windows**, **macOS** (x64 & arm64), and **Linux** (x64 & ARM, including Raspberry Pi).

![v20](https://github.com/awawa-dev/HyperHDR/assets/69086569/9bc7999d-1515-4a96-ba5e-8a732cf7d8a4)

## Main Features of HyperHDR

At the heart of HyperHDR lies the **Infinite Color Engine** ( :new: HyperHDR v22), our own in-house rendering pipeline designed for maximum precision and visual fidelity. By leveraging floating-point processing, it produces smoother gradients, eliminates rounding artifacts, and ensures stable, consistent color transformations. While many other solutions rely on basic 24-bit color operations that introduce precision loss and visible banding, the Infinite Color Engine achieves uncompromised accuracy and professional-grade results. With deep-color support for Philips Hue, LIFX and HD108 devices, HyperHDR provides richer, more vibrant illumination than ever before.  

The Infinite Color Engine has also paved the way for our bespoke, internally developed RGB-to-RGBW conversion, featuring energy-preserving extraction, temperature-aware mapping, and temporal dithering with motion-adaptive diffusion and hysteresis to eliminate flickering.  

### Key advantages of the Infinite Color Engine ( :new: HyperHDR v22):
* **Floating-Point Precision:** All color computations use high-precision floating-point arithmetic, eliminating cumulative rounding errors for more accurate results
* **Linear sRGB Accuracy:** Core color transformations are processed in linear sRGB space, ensuring physically correct and consistent light reproduction
* **Deep-Color Support:** Compatible devices, including Philips Hue lamps, LIFX and HD108 LEDs, can take advantage of rendering beyond standard 24-bit RGB color depth
* **Advanced Color Smoothing Algorithms:** Inertial-physics, exponential, and perceptually-uniform YUV/RGB interpolators for more fluid and natural color transitions
* **High-precision RGB-to-RGBW conversion:** Energy-aware power balancing, white point temperature calibration, temporal dithering and anti-flicker hysteresis

### Additional features:
* **Ultra-low CPU usage** on SoCs like Raspberry Pi or Intel N100  
* **Lightweight design** with no heavy dependencies (e.g. no Python or Java)  
* **Low-latency video processing** for LED strips and lamps  
* **Optimized multithreading**, enabling Raspberry Pi to process high-quality video streams  
* **High portability** across ARM-based embedded platforms  
* **System diagnostics:** live CPU/RAM usage, CPU temperature, undervoltage detection, USB grabber and LED performance  
* **USB grabber support** on Linux, Windows 10/11, and macOS for P010/NV12/YUYV/MJPEG/UYVY/I420/RGB   
* **Hardware-accelerated capture:** PipeWire/Portal (Linux/Wayland), DirectX (Windows 10/11)  
* **HDR-ready DirectX screen grabbing:** Supports DXGI_FORMAT_R16G16B16A16_FLOAT and multiple monitors 
* **Optimized video processing:** Our pipeline smoothly handles 1080p **P010**/**NV12**/**YUYV** even on Rpi4
* **Built-in audio visualization** powered by spectrum analysis  
* **MQTT support** for IoT integration  
* **Home Assistant and zigbee2mqtt integration**  
* **Automatic tone mapping** for SDR/HDR content  
* **Automatic LUT calibration** for optimal HDR/SDR grabber quality using MP4 test files  
* **Latency benchmarking** for USB grabbers  
* **P010 support** on Windows & Linux (our patched Raspberry Pi OS image - P010 is not supported in mainline OS)
* **Intuitive LED strip editor**, with automatic or manual geometry editing via mouse and context menus  
* **Smart signal detection** with adaptive learning for USB grabbers  
* **External tone mapping support** for flatbuffers/protobuf sources  
* **Wide LED strip compatibility** including WS281x, APA102, HD107, SK9822, SK6812 and our ultra-fast LED controllers:  
  * [HyperSPI](https://github.com/awawa-dev/HyperSPI) for ESP8266/ESP32/RP2040  
  * [HyperSerialEsp8266](https://github.com/awawa-dev/HyperSerialEsp8266), [HyperSerialESP32](https://github.com/awawa-dev/HyperSerialESP32), [HyperSerialPico](https://github.com/awawa-dev/HyperSerialPico) USB serial port 2Mb+ speed connection    
  * :new: [Hyperk](https://github.com/awawa-dev/Hyperk): our optimized wireless LED controller for ESP8266/ESP32 (incl. S2/S3/C2/C3/C5/C6) and Raspberry Pi Pico W (RP2040/RP2350) family

HyperHDR’s advanced video pipeline significantly enhances LED output, creating a smoother, more immersive ambient lighting experience. It works with SDR, HDR, and Dolby Vision (Low Latency Dolby Vision / LLDV only, if supported by your hardware). Instead of relying on USB grabbers, you can also use software screen capture directly from your PC.  

![example](https://github.com/awawa-dev/HyperHDR/assets/69086569/4077c05d-4c02-47eb-8d64-a334064403b3)

## Downloads

**Official releases:**  
[https://github.com/awawa-dev/HyperHDR/releases](https://github.com/awawa-dev/HyperHDR/releases)

Windows installers are code-signed by [SignPath Foundation](https://signpath.org). See our [Code Signing Policy](CODE_SIGNING_POLICY.md).

**Official Linux repository:**  
[https://awawa-dev.github.io/](https://awawa-dev.github.io/)

**Latest development build:**  
[Download the latest test build](https://github.com/awawa-dev/HyperHDR/releases/tag/latest-build)

Fresh daily builds shipped at π time (3:14 UTC) whenever we have something new.

## Documentation

👉 [Explore Our Wiki](https://wiki.hyperhdr.eu/) 👈

## Community

[HyperHDR Support Forum](https://github.com/awawa-dev/HyperHDR/discussions)

## How to Compile HyperHDR from Source

[Compiling HyperHDR](https://awawa-dev.github.io/wiki/Compiling-HyperHDR.html)

## In the Press

<img align="left" width="286" height="200" src="https://i.postimg.cc/zvr9rWR4/magazine.jpg"/>
<a href="https://makezine.com/projects/bright-lights-big-tv-diy-ambient-lights/">Make: Magazine #84 (2023)</a><br>
<a href="https://magpi.raspberrypi.com/issues/117">MagPi #117 (2022)</a><br>
<a href="https://web.archive.org/web/20230824230034/https://www.smartprix.com/bytes/what-is-bias-lighting-philips-hue-ambient-light-vs-govee-dreamview-tv-backlight-vs-diy-ambient-light-with-hyperhdr/">Comparison of modern ambient lighting systems (2023)</a><br>
<a href="https://www.raspberrypi.com/tutorials/raspberry-pi-tv-ambient-lighting">Tutorial on raspberrypi.com</a><br>
<a href="https://www.youtube.com/watch?v=4jkwFsMkKwU">Building a 4K HDMI TV Backlight (2021)</a><br><br><br><br><br>

## License

Released under the MIT License  
[![GitHub license](https://img.shields.io/badge/License-MIT-yellow.svg)](https://raw.githubusercontent.com/awawa-dev/HyperHDR/master/LICENSE)

---

# HyperHDR v24.0.0-beta.0 — Native Android TV Ambient Lighting App

Pure standalone native Android TV application (`org.hyperhdr.androidtv`), bundling all **226 Web UI files** (`web_resources.rcc`) and **123 embedded JSON configuration schemas** (including new HID drivers `lightpack`, `paintpack`, `rawhid`, `synclight` under `leds_group_6_HID` and `sp616e` serial driver) with dual-engine Android TV screen grabbing (`MediaCodec` hardware pipeline & `MediaProjection` API, fallback `screencap`, `FlatBuffers :19400`, `Protobuf :19445`, and `JSON-RPC image :19444`), the Infinite Color Engine, and WLED UDP (`21324` DRGB/DNRGB & `4048` DDP) + LAN discovery.

## Screen Capture Architecture & Features (v24.0.0-beta.0)

* **Dual-Engine Screen Grabber**:
  * **MediaCodec Hardware Pipeline (Default)**: Uses Android's native hardware video surfaces with lowest capture resolution (`64x36`) and zero-allocation reusable pixel buffers, eliminating video stutter when playing 1080p VP9 or AV1 video on Android TV SoCs.
  * **ImageReader Surface Capture**: Direct `RGBA_8888` pixel reading with forced opaque alpha (`0xFF000000`) to prevent black frames on `SurfaceFlinger` hardware video overlays.
  * **TV Remote Grabber Switcher**: Switch capture engines or toggle between 64x36 and 160x90 capture resolution directly via TV remote control or the Web UI.
* **Automatic `MediaProjection` Consent & Multi-Flag VirtualDisplay**: Automatically prompts for Android TV `MediaProjection` permission on launch (or via the **Start TV Capture** button) and creates the `VirtualDisplay` with `VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR | VIRTUAL_DISPLAY_FLAG_PUBLIC | VIRTUAL_DISPLAY_FLAG_PRESENTATION` (with automatic fallback) so all TV surfaces mirror reliably.
* **Native `screencap` Fallback + External Network Grabbers**: Includes a background `/system/bin/screencap` raw frame fallback if `MediaProjection` is idle, plus full TCP listeners for **FlatBuffers (`:19400`)**, **Protobuf (`:19445`)**, and **JSON-RPC `image` (`:19444` / `:8090`)**.
* **Zero External Dependencies**: Runs entirely within the Android TV APK with built-in HTTP/WebSocket Web UI server on port `8090`. No Termux, root, or command-line daemons required.

## Default Password & Authentication Behavior

* **Default First-Time Password**: `hyperhdr`
* **First-Time Login Flow**:
  * On a fresh installation (while the password is still the default `hyperhdr`), `authorize-newPasswordRequired` returns `true`. The Web UI automatically logs in using `hyperhdr`, displays the warning banner prompting you to change the default password, and pre-fills `hyperhdr` as the current password in the **Change Password** dialog.
* **After Changing the Password**:
  * Change the password via the sidebar **Change Password** button (minimum 8 characters).
  * Once changed, `defaultPasswordSet` becomes `false` and all previous session tokens are immediately invalidated.
  * Every new browser session, page reload after locking (`Lock UI`), or unauthenticated WebSocket connection is required to sign in on the **Login** screen with your new password before any configuration or server commands (`config`, `serverinfo`, `instance`, `componentstate`, etc.) are allowed.
* **Resetting Forgotten Password**:
  * Clear app data in Android TV Settings (`adb shell pm clear org.hyperhdr.androidtv`) or delete `/data/data/org.hyperhdr.androidtv/files/hyperhdr_db.json`.

## Build and Installation

* **Signed APK**: `HyperHDR-v24.0.0-beta.0-AndroidTV.apk`
* **Rebuild APK**:
  ```bash
  ./build_apk.sh
  ```
* **Install via ADB**:
  ```bash
  adb connect <tv-ip>:5555
  adb install -r HyperHDR-v24.0.0-beta.0-AndroidTV.apk
  ```
