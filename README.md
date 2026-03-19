![C++20](https://img.shields.io/badge/Language-C%2B%2B20-blue) ![CMake](https://img.shields.io/badge/Build%20System-CMake-orange) ![MIT](https://img.shields.io/badge/License-MIT-brightgreen) ![Objective‑C++](https://img.shields.io/badge/Language-Objective‑C%2B%2B-lightblue) ![JavaScript](https://img.shields.io/badge/Language-JavaScript-teal) ![HTML](https://img.shields.io/badge/Language-HTML-orange) ![Linear Algebra](https://img.shields.io/badge/Linear%20Algebra-Vector%20Computations-maroon) ![JSON API](https://img.shields.io/badge/JSON%20API-supported-blue) ![Bootstrap 5](https://img.shields.io/badge/UI‑Framework-Bootstrap%205-purple) ![DirectX](https://img.shields.io/badge/DirectX-supported-lightblue) ![Wayland‑PipeWire](https://img.shields.io/badge/Wayland‑PipeWire-supported-teal) ![Sound Reactive](https://img.shields.io/badge/Sound-Reactive-brightgreen) ![MQTT](https://img.shields.io/badge/MQTT-supported-orange) ![mDNS‑Zeroconf](https://img.shields.io/badge/mDNS‑Zeroconf-supported-lightblue) ![USB Grabbers](https://img.shields.io/badge/USB%20Grabbers-enabled-blueviolet) ![Raspberry Pi ARM](https://img.shields.io/badge/Raspberry%20Pi‑ARM-64bit-brightgreen) ![x64](https://img.shields.io/badge/Architecture-x64-blue) ![ARM](https://img.shields.io/badge/Architecture-ARM-orange) ![WLED](https://img.shields.io/badge/WLED-supported-brightgreen) ![Home Assistant](https://img.shields.io/badge/Home%20Assistant-supported-orange) ![Adalight](https://img.shields.io/badge/Adalight-supported-purple)

## About

HyperHDR is an open-source ambient lighting system for TVs and music setups, based on real-time video and audio stream analysis. It is designed with a strong focus on stability, high performance, and high-fidelity video decoding and mapping for precise and vibrant LED visuals. Optimized for both single-threaded and multi-threaded video processing, HyperHDR runs seamlessly on **Windows**, **macOS** (x64/arm64: M1, M2), and **Linux x64 & ARM** (including Raspberry Pi).

![v20](https://github.com/awawa-dev/HyperHDR/assets/69086569/9bc7999d-1515-4a96-ba5e-8a732cf7d8a4)

## Main Features of HyperHDR

At the heart of HyperHDR lies the **Infinite Color Engine** ( :new: HyperHDR v22), our own in-house rendering pipeline designed for maximum precision and visual fidelity. By leveraging floating-point processing, it produces smoother gradients, eliminates rounding artifacts, and ensures stable, consistent color transformations. While many other solutions rely on basic 24-bit color operations that introduce precision loss and visible banding, the Infinite Color Engine achieves uncompromised accuracy and professional-grade results. With deep-color support for Philips Hue, LIFX and HD108 devices, HyperHDR provides richer, more vibrant illumination than ever before.  

The Infinite Color Engine has also paved the way for our bespoke, internally developed RGB-to-RGBW conversion, featuring energy-preserving extraction, temperature-aware mapping, and temporal dithering with motion-adaptive diffusion and hysteresis to eliminate flickering.  

### Key advantages of the Infinite Color Engine ( :new: HyperHDR v22):
* **Floating-Point Precision:** All color computations use high-precision floating-point arithmetic, eliminating cumulative rounding errors for more accurate results
* **Linear sRGB Accuracy:** Core color transformations are processed in linear sRGB space, ensuring physically correct and consistent light reproduction
* **Deep-Color Support:** Compatible devices, including Philips Hue lamps, LIFX and HD108 LEDs, can take advantage of rendering beyond standard 24-bit RGB color depth.
* **Advanced color smoothing algorithms:** Inertia-based physics, exponential, and perceptually-uniform YUV/RGB interpolators for more fluid and natural color transitions
* **High-precision RGB-to-RGBW conversion:** Energy-aware power balancing, white point temperature calibration, temporal dithering and anti-flicker hysteresis

### Additional features:
* **Ultra-low CPU usage** on SoCs like Raspberry Pi or Intel N100  
* **Lightweight design** with no heavy dependencies (e.g. no Python or Java)  
* **Low-latency video processing** for LED strips and lamps  
* **Optimized multithreading**, enabling Raspberry Pi to process high-quality video streams  
* **High portability** across ARM-based embedded platforms  
* **System diagnostics:** live CPU/RAM usage, CPU temperature, undervoltage detection, USB grabber and LED performance  
* **USB grabber support** for Linux, Windows 10/11, and macOS for P010/NV12/YUYV/MJPEG/UYVY/I420/RGB   
* **Hardware-accelerated capture:** PipeWire/Portal (Linux/Wayland), DirectX (Windows 10/11)  
* **HDR-ready DirectX screen grabbing:** Supports DXGI_FORMAT_R16G16B16A16_FLOAT and multiple monitors 
* **Optimized video processing:** Our optimized pipeline handles smoothly 1080p **P010**/**NV12**/**YUYV** even on Rpi4
* **Built-in audio visualization** powered by spectrum analysis  
* **MQTT support** for IoT integration  
* **Home Assistant and zigbee2mqtt integration**  
* **Automatic tone mapping** for SDR/HDR content  
* **Automatic LUT calibration** for optimal HDR/SDR grabber quality using MP4 test files  
* **Latency benchmarking** for USB grabbers  
* **P010 support** for Windows, Linux: our patched Raspberry Pi OS image (P010 is unsupported in mainline OS)
* **Intuitive LED strip editor**, with automatic or manual geometry editing via mouse and context menus  
* **Smart signal detection** with adaptive learning for USB grabbers  
* **External tone mapping support** for flatbuffers/protobuf sources  
* **Wide LED strip compatibility** and for WS281x, APA102, HD107, SK9822, SK6812 our ultra-fast LED controllers:  
  * [HyperSPI](https://github.com/awawa-dev/HyperSPI) for ESP8266/ESP32/rp2040  
  * [HyperSerialEsp8266](https://github.com/awawa-dev/HyperSerialEsp8266), [HyperSerialESP32](https://github.com/awawa-dev/HyperSerialESP32), [HyperSerialPico](https://github.com/awawa-dev/HyperSerialPico) USB serial port 2Mb+ speed connection    
  * :new: [Hyperk](https://github.com/awawa-dev/Hyperk): our optimized wireless LED controller for esp8266/ESP32 (inc. S2/S3/C2/C3/C5/C6) and Raspberry Pi Pico W (rp2040/rp2350) family

HyperHDR’s advanced video pipeline significantly enhances LED output, creating a smoother, more immersive ambient lighting experience. It works with SDR, HDR, and Dolby Vision (LLDV if supported by your hardware). Instead of relying on USB grabbers, you can also use software screen capture directly from your PC.  

![example](https://github.com/awawa-dev/HyperHDR/assets/69086569/4077c05d-4c02-47eb-8d64-a334064403b3)

## Downloads

Official releases:  
[https://github.com/awawa-dev/HyperHDR/releases](https://github.com/awawa-dev/HyperHDR/releases)

Official Linux repository:  
[https://awawa-dev.github.io/](https://awawa-dev.github.io/)

Latest test builds (GitHub Action — login required, select latest build from master branch, setups in ZIP artifacts):  
![Latest build on master](https://github.com/awawa-dev/HyperHDR/actions?query=branch%3Amaster)

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
