## About 

Open source ambient lighting implementation for television and music sets based on the video and audio streams analysis. Focused on stability while ensuring performance and quality, especially for USB grabbers. Single and multi-threaded video processing optimization for **Windows**, **macOS** and **Linux x64 & ARM** (Raspberry Pi and others).
  
![v20](https://github.com/awawa-dev/HyperHDR/assets/69086569/9bc7999d-1515-4a96-ba5e-8a732cf7d8a4)

## Main features of HyperHDR

* **Really low CPU** usage on SoCs like Raspberry Pi
* Lightweight, we don't use bindings to heavy components like Python or Java
* Low latency video processing as color source for LED strip and lamps
* Support for multithreading that makes Raspberry Pi capable of processing HQ video stream
* High portability on various ARM embedded platforms
* Video post-processing filter to eliminate LED flickering
* Modern interface using Bootstrap 5 and SVG icons
* Provides vital informations about your OS condition: CPU & RAM usage, CPU temperature, undervoltage detection, internal components performance including USB grabber and LED devices
* Support for USB grabbers under Linux, Windows 10, macOS (x64/M1)
* Pipewire/Portal hardware-accelerated screen capturer for Linux/Wayland
* DirectX screen grabber with pixel and vertex shader processing acceleration for Windows 10/11
* Dynamic video cache buffers. Now Rpi can process even 1080p120 NV12 stream without any size decimation
* Built-in audio visualization effects using **spectrum analysis**
* MQTT support for IoT
* Entertainment API v2: per-channel support for Philips Gradient Strip and others
* Automatic LUT calibration detects grabber model specific properties for the best quality of HDR/SDR
* Optimized multi-instances. You can use for example your TV LED strip and multiple WLED or Philips Hue light sources.
* Built-in latency benchmark for USB grabbers
* Software screen grabbers: DirectX11 (Windows), CoreGraphics (macOS), Pipewire/Wayland/x11 (Linux), frame buffer (Linux)
* easy LED strip geometry editing process, automatic or manual with mouse and context menu per single LED
* Automatic signal detection with smart learning capability for USB grabbers
* SK6812 RGBW: advanced calibration of the white color channel for [HyperSerialEsp8266](https://github.com/awawa-dev/HyperSerialEsp8266), [HyperSerialESP32](https://github.com/awawa-dev/HyperSerialESP32), [HyperSPI](https://github.com/awawa-dev/HyperSPI), [HyperSerialPico](https://github.com/awawa-dev/HyperSerialPico)
* Tone mapping for external flatbuffers/protobuf sources
* Built-in LUT table generator and LUT import tool
* Support for 48bits HD108 LED strip
* Support WS821x, APA102, HD107, SK9822 and SK6812 RGBW LED strips using the fastest possible cable solution for generic ESP8266/ESP32/rp2040 external LED drivers: [HyperSPI](https://github.com/awawa-dev/HyperSPI). Alternatively, there is a simple solution using a high-speed connection via a standard USB serial port for ESP8266/ESP32/Pico with data integrity check and white channel calibration: [HyperSerialEsp8266](https://github.com/awawa-dev/HyperSerialEsp8266) and [HyperSerialESP32](https://github.com/awawa-dev/HyperSerialESP32) and for Raspberry Pi Pico [HyperSerialPico](https://github.com/awawa-dev/HyperSerialPico). You can also use our WLED fork for ESP8266 and ESP32 using 2Mb baud speed (or higher): [HyperSerialWLED](https://github.com/awawa-dev/HyperSerialWLED)

Our advanced video processing can improve the source for the LEDs, making the ambient lighting even more enjoyable and colorful.
![example](https://github.com/awawa-dev/HyperHDR/assets/69086569/4077c05d-4c02-47eb-8d64-a334064403b3)

## Download packages & sources

Official releases:  
[https://github.com/awawa-dev/HyperHDR/releases](https://github.com/awawa-dev/HyperHDR/releases)

Official Linux repo:  
[https://awawa-dev.github.io/](https://awawa-dev.github.io/)

The latest installers for testing can usually be found as artifact archives in the Github Action tab (must be logged in):  
https://github.com/awawa-dev/HyperHDR/actions

## Manuals

[Installation manual](https://github.com/awawa-dev/HyperHDR/wiki/Installation)

[Official Wiki](https://github.com/awawa-dev/HyperHDR/wiki)  
  
[Example of how to build SK6812 RGBW based ambient lighting system - updated 2023](https://www.hyperhdr.eu/2023/02/ultimate-guide-on-how-to-build-led.html)

## Forum

[HyperHDR's support section](https://github.com/awawa-dev/HyperHDR/discussions)

## How to compile

[Building HyperHDR from sources](https://github.com/awawa-dev/HyperHDR/wiki/Compiling-HyperHDR)

## Press mentions

<img align="left" width="286" height="200" src="https://i.postimg.cc/zvr9rWR4/magazine.jpg"/>
<a href="https://makezine.com/projects/bright-lights-big-tv-diy-ambient-lights/">Make: Magazine #84 (2023)</a><br>
<a href="https://magpi.raspberrypi.com/issues/117">MagPi #117 (2022)</a><br>
<a href="https://web.archive.org/web/20230824230034/https://www.smartprix.com/bytes/what-is-bias-lighting-philips-hue-ambient-light-vs-govee-dreamview-tv-backlight-vs-diy-ambient-light-with-hyperhdr/">Comparison of several modern ambient lighting systems (2023)</a><br>
<a href="https://www.raspberrypi.com/tutorials/raspberry-pi-tv-ambient-lighting">The tutorial on raspberrypi.com</a><br>
<a href="https://www.youtube.com/watch?v=4jkwFsMkKwU">Building a 4K Capable HDMI TV Backlight (2021)</a><br><br><br><br><br>

## License
  
The source is released under MIT-License (see http://opensource.org/licenses/MIT)  
[![GitHub license](https://img.shields.io/badge/License-MIT-yellow.svg)](https://raw.githubusercontent.com/awawa-dev/HyperHDR/master/LICENSE)
