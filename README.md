## About

HyperHDR is an open-source ambient lighting system for TVs and music setups, based on real-time video and audio stream analysis. It is designed with a strong focus on stability, high performance, and superior image quality. Optimized for both single-threaded and multi-threaded video processing, HyperHDR runs seamlessly on **Windows**, **macOS** (x64/arm64: M1, M2), and **Linux x64 & ARM** (including Raspberry Pi).

![v20](https://github.com/awawa-dev/HyperHDR/assets/69086569/9bc7999d-1515-4a96-ba5e-8a732cf7d8a4)

## Main Features of HyperHDR

At the heart of HyperHDR lies the **Infinite Color Engine**, our own in-house rendering pipeline designed for maximum precision and visual fidelity. By leveraging floating-point processing, it produces smoother gradients, eliminates rounding artifacts, and ensures stable, consistent color transformations. While many other solutions rely on basic 24-bit color operations that introduce precision loss and visible banding, the Infinite Color Engine achieves uncompromised accuracy and professional-grade results. With deep-color support for Philips Hue and HD108 devices, HyperHDR provides richer, more vibrant illumination than ever before. ( :new: HyperHDR v22)  

### Key advantages of the Infinite Color Engine:
* **Floating-Point Precision:** All color computations use high-precision floating-point arithmetic, eliminating cumulative rounding errors for more accurate results
* **Linear sRGB Accuracy:** Core color transformations are processed in linear sRGB space, ensuring physically correct and consistent light reproduction
* **Deep-Color Support:** Compatible devices, including Philips Hue lamps and HD108 LEDs, can take advantage of rendering beyond standard 24-bit RGB color depth.
* **Advanced color smoothing algorithms:** inertia-based physics, exponential, and perceptually-uniform YUV/RGB interpolators for more fluid and natural color transitions

### Additional features:
* **Ultra-low CPU usage** on SoCs like Raspberry Pi or Intel N100  
* **Lightweight design** with no heavy dependencies (e.g., Python, Java)  
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
* **Advanced SK6812 RGBW calibration** for [HyperSerialEsp8266](https://github.com/awawa-dev/HyperSerialEsp8266), [HyperSerialESP32](https://github.com/awawa-dev/HyperSerialESP32), [HyperSPI](https://github.com/awawa-dev/HyperSPI), and [HyperSerialPico](https://github.com/awawa-dev/HyperSerialPico)  
* **External tone mapping support** for flatbuffers/protobuf sources  
* **Wide LED strip compatibility** and for WS281x, APA102, HD107, SK9822, SK6812 our ultra-fast LED controllers:  
  * [HyperSPI](https://github.com/awawa-dev/HyperSPI) for ESP8266/ESP32/rp2040  
  * [HyperSerialEsp8266](https://github.com/awawa-dev/HyperSerialEsp8266), [HyperSerialESP32](https://github.com/awawa-dev/HyperSerialESP32), [HyperSerialPico](https://github.com/awawa-dev/HyperSerialPico) USB serial port 2Mb+ speed connection    
  * [HyperSerialWLED](https://github.com/awawa-dev/HyperSerialWLED): our optimized WLED fork with USB serial port 2Mb+ speed connection

HyperHDR’s advanced video pipeline significantly enhances LED output, creating a smoother, more immersive ambient lighting experience. It works with SDR, HDR, and Dolby Vision (LLDV if supported by your hardware). Instead of relying on USB grabbers, you can also use software screen capture directly from your PC.  

![example](https://github.com/awawa-dev/HyperHDR/assets/69086569/4077c05d-4c02-47eb-8d64-a334064403b3)

## Downloads

Official releases:  
[https://github.com/awawa-dev/HyperHDR/releases](https://github.com/awawa-dev/HyperHDR/releases)

Official Linux repository:  
[https://awawa-dev.github.io/](https://awawa-dev.github.io/)

Latest test builds (GitHub login required):  
[https://github.com/awawa-dev/HyperHDR/actions](https://github.com/awawa-dev/HyperHDR/actions)

## Manuals & Guides

[Installation manual](https://github.com/awawa-dev/HyperHDR/wiki/Installation)  
[Official Wiki](https://github.com/awawa-dev/HyperHDR/wiki)  
[Complete guide to building an SK6812 RGBW ambient lighting system (2023)](https://www.hyperhdr.eu/2023/02/ultimate-guide-on-how-to-build-led.html)

## Community

[HyperHDR Support Forum](https://github.com/awawa-dev/HyperHDR/discussions)

## Building from Source

[Compilation guide](https://github.com/awawa-dev/HyperHDR/wiki/Compiling-HyperHDR)

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
