## About 

Open source ambient lighting implementation for television sets based on the video and audio streams analysis, using performance improvements especially for USB grabbers. Single and multi-threaded video processing optimization for **Windows**, **macOS** and **Linux x64 & ARM** (Raspberry Pi and others) for SDR/HDR streams captured by USB grabbers using pre-calculated LUT tables. Direct support for USB grabbers under Windows 10 (Microsoft Media Foundation), Linux (v4l2) and macOS (AVFoundation). Also software screen grabbers are available.  
  
![v17](https://user-images.githubusercontent.com/69086569/131157173-cae41f0e-d5c3-413c-ba6b-041e8bfc0017.png)
  
## Download packages & sources

Official releases:  
https://github.com/awawa-dev/HyperHDR/releases

Official Linux repo:    
https://awawa-dev.github.io/

Installation manual:  
https://github.com/awawa-dev/HyperHDR/wiki/Installation

The latest test installers can usually be found as artifact archives in the Github Action tab (must be logged in):  
https://github.com/awawa-dev/HyperHDR/actions

Default LUT table is already included, but for the best effect you can generate your own using new calibration tool. Or you may download dedicated LUT tables for supported USB grabbers: [link](https://www.hyperhdr.eu/2022/04/usb-grabbers-hdr-to-sdr-quality-test.html#chapter4). HyperHDR v20 and above allow direct one-click installation from the interface. You can also import and convert custom 3DL Autodesk LUT table using HyperHDR built-in tool. 

**Using proper LUT table for your USB grabber is very important both for HDR and SDR content!** :warning: 

## How to compile

[Building HyperHDR from sources](https://github.com/awawa-dev/HyperHDR/wiki/Compiling-HyperHDR)
  
## Support and contact

[HyperHDR's support section](https://github.com/awawa-dev/HyperHDR/discussions)
  
**Manuals and guides for building and configuring your own ambient lighting system with HyperHDR:**

[Official Wiki](https://github.com/awawa-dev/HyperHDR/wiki)  
  
[How to build SK6812 RGBW based ambient lighting system - updated 2023](https://www.hyperhdr.eu/2023/02/ultimate-guide-on-how-to-build-led.html)
  
## Main features of HyperHDR:

* **Really low CPU** usage on SoCs like Raspberry Pi
* Support for multithreading that makes Raspberry Pi capable of processing HQ video stream
* High portability on various ARM embedded platforms
* Video post-processing filter to eliminate LED flickering
* Built-in automatic HDR LUT calibration for USB grabbers
* Automatic LUT calibration also detects grabber model specific properties for the best quality of HDR/SDR
* Built-in latency benchmark for USB grabbers
* HDR/BT2020 color, treshold & gamma correction (LUT)
* Provides vital informations about your OS condition: CPU & RAM usage, CPU temperature, undervoltage detection, internal components performance including USB grabber and LED devices
* Automatic signal detection with smart learning capability for USB grabbers
* Support for USB grabbers under Linux, Windows 10, macOS (x64/M1)
* Software screen grabbers: DirectX11 (Windows), CoreGraphics (macOS), Pipewire/Wayland/x11 (Linux), frame buffer (Linux)
* HDR tone mapping for external flatbuffers/protobuf sources
* Built-in LUT table generator and LUT import tool
* Built-in audio visualization effects using spectrum analysis
* Entertainment API v2: per-channel support for Philips Gradient Strip and others
* Dynamic video cache buffers. Now Rpi can process even 1080p120 NV12 stream without any size decimation
* SK6812 RGBW: the white color channel calibration for [HyperSerialEsp8266](https://github.com/awawa-dev/HyperSerialEsp8266), [HyperSerialESP32](https://github.com/awawa-dev/HyperSerialESP32), [HyperSPI](https://github.com/awawa-dev/HyperSPI)
* Optimized multi-instances. You can use for example your TV LED strip and multiple WLED or Philips Hue light sources.
* Support WS821x, APA102/HD107/SK9822 and SK6812 RGBW LED strips using the fastest possible cable solution for generic ESP8266/ESP32/rp2040 external LED drivers: [HyperSPI](https://github.com/awawa-dev/HyperSPI) Alternative there is also an implementation of AWA ultra-fast serial USB protocol for ESP8266/ESP32 using 2Mb baud speed (or higher) with data integrity check and white channel calibration: [HyperSerialEsp8266](https://github.com/awawa-dev/HyperSerialEsp8266) and [HyperSerialESP32](https://github.com/awawa-dev/HyperSerialESP32) and for Raspberry Pi Pico [HyperSerialPico](https://github.com/awawa-dev/HyperSerialPico) You can also use our WLED fork for ESP8266 and ESP32 using 2Mb baud speed (or higher): [HyperSerialWLED](https://github.com/awawa-dev/HyperSerialWLED)

You don't need to use an HDR source to use HyperHDR. You can just benefit from: high performance & optimized video proccessing, capability of using multithreading and optimized video pipeline to avoid bottleneck resources, support for modern USB grabbers under Windows 10/Linux/macOS, decicated fast USB LED drivers (HyperSerialEsp8266, HyperSerialESP32, HyperSerialPico and HyperSerialWLED) or SPI driver (HyperSPI), advanced calibration of RGBW sk6812 LED strip, screen grabbers, easy to setup automatic video signal detection, music capabilities and anti-flickering filter for best movie experience. You can easily build simple POST/GET commands to remotely control many HyperHDR features using our JSON API Playground: just choose the desired action, test it, use it.

For the best video quality, you can create your own calibrated LUT table using HyperHDR or use LUT dedicated for USB grabbers: MS2109 and MS2130 clones, Navy U3, Ezcap 269, Ezcap 320, Ezcap 321, Ezcap 331, AV Access 4KVC00, Elgato HD60X [link](https://www.hyperhdr.eu/2022/04/usb-grabbers-hdr-to-sdr-quality-test.html#chapter4). Why is it worth using them? [USB grabbers HDR to SDR quality review](https://www.hyperhdr.eu/2022/04/usb-grabbers-hdr-to-sdr-quality-test.html) The latest HyperHDR v20 offers a tool to automatically download and install them with one click. Since certain properties of some of the video codecs used by the grabber that are needed to decode it are not included in the standard and are detected during auto calibration, you need these LUTs even if you are not using an HDR signal. For example, MS2109/MS2130 clones behave differently than most other grabbers on the market offering full YUV scale.

Although HyperHDR capabilities are not limited to HDR signal, here is the result of HyperHDR processing of the captured raw HDR/BT.2020 video stream. Without it, your ambient lighting colors will be washed-out.  

![1](https://github.com/awawa-dev/HyperHDR/assets/69086569/783b56e0-86f3-458b-a441-91ebdfff6756)
![2](https://github.com/awawa-dev/HyperHDR/assets/69086569/49d43111-950b-428f-91d3-ac0cc1e3274b)
![3](https://github.com/awawa-dev/HyperHDR/assets/69086569/3e8dbc4f-844a-411c-87cf-f7c8ed9155e9)
![4](https://github.com/awawa-dev/HyperHDR/assets/69086569/4077c05d-4c02-47eb-8d64-a334064403b3)

## License
  
The source is released under MIT-License (see http://opensource.org/licenses/MIT).  
[![GitHub license](https://img.shields.io/badge/License-MIT-yellow.svg)](https://raw.githubusercontent.com/awawa-dev/HyperHDR/master/LICENSE)
