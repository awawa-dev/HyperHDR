## About 

Open source ambient lighting implementation for television sets based on the video and audio streams analysis, using performance improvements especially for USB grabbers. Single and multi-threaded video processing optimization for **Windows**, **macOS** and **Linux x64 & ARM** (Raspberry Pi and others) for SDR/HDR streams captured by USB grabbers using pre-calculated LUT tables. Direct support for USB grabbers under Windows 10 (Microsoft Media Foundation), Linux (v4l2) and macOS (AVFoundation). Also software screen grabbers are available now :new:  
  
  ![v17](https://user-images.githubusercontent.com/69086569/131157173-cae41f0e-d5c3-413c-ba6b-041e8bfc0017.png)
  
## Download packages & sources

Official releases: \
https://github.com/awawa-dev/HyperHDR/releases

Latest testing installers can be find as the artifacts of the latest build in the Github Action tab at the bottom of the page. Must be logged in. \
https://github.com/awawa-dev/HyperHDR/actions

Default LUT table is already included (in the installers) but you can generate your own or import 3dl Autodesk lut table.\
For Raspberry Pi you may use prepared SD card images: [manual](https://hyperhdr.blogspot.com/2020/11/hyperhdr-prepare-for-building-buying_17.html)\
Default hostname for SD images is HyperHDR so connect to http://hyperhdr:8090/ \
SSH and SPI are enabled on default.

## How to compile

[Building HyperHDR from sources](https://github.com/awawa-dev/HyperHDR/wiki/Compiling-HyperHDR) :new:
  
## Support and contact

[HyperHDR's support section](https://github.com/awawa-dev/HyperHDR/discussions) (or https://hyperhdr.blogspot.com/ )
  
Discord: https://discord.gg/gvyBnnns :new:  
  
**Manuals and guides for building and configuring your own ambient lighting system with HyperHDR:**

[Official Wiki](https://github.com/awawa-dev/HyperHDR/wiki) :new:  
  
[Build-log from my SK6812 RGBW system and one thing about calibration](https://hyperhdr.blogspot.com/2020/12/my-build-log-using-sk6812-rgbw-led.html)
  
## Main features of HyperHDR:

* **Really low CPU** usage on SoCs like Raspberry Pi using v4l2 grabbers
* Support for multithreading that makes Raspberry Pi capable of processing HQ video stream (Rpi 1 & Zero should also benefit from the optimization alone)
* Built-in LUT table generator
* HDR/BT2020 color, treshold & gamma correction (LUT)
* SDR treshold & gamma correction for selected codecs (LUT)
* Automatic signal detection with smart learning capability for USB grabbers :new:
* Support for USB grabbers under Windows 10
* Support for USB grabbers under macOS (x64/M1)
* Support for software screen grabbers: DirectX11 (Windows), CoreGraphics (macOS), X11 (Linux) :new:
* Built-in audio visualization effects
* Optimized multi-instances. You can use for example your TV LED strip and multiple WLED or Philips Hue light sources.
* Support for WS821x, APA102 (HD107, SK9822 etc) and SK6812 RGBW LED strips using fastest possible cable solution for generic ESP8266/ESP32 external LED drivers: [HyperSPI](https://github.com/awawa-dev/HyperSPI) :new:
* Support for WS8201, WS821x, APA102 (HD107, SK9822 etc) and SK6812 RGBW LED strips ultrafast USB serial port AWA protocol for ESP8266/ESP32 at 2Mb baud with data integrity check and white channel calibration: [HyperSerialEsp8266](https://github.com/awawa-dev/HyperSerialEsp8266) and [HyperSerialESP32](https://github.com/awawa-dev/HyperSerialESP32) :new:
* WLED fork for ESP8266 & ESP32 at 2Mb baud (also @921600 baud :new:) and almost all popular types of LED strips is available: [HyperSerialWLED](https://github.com/awawa-dev/HyperSerialWLED)

##

**Changelog (v17)** :new:
- Overall performance without tone mapping for USB grabbers improved x10 (MJPEG) and x3 (YUV) over Hyperion NG 2.0.0.8A thanks to optimization & using of multi-threading
- Direct support for USB grabbers under Windows 10, Linux and macOS (really fast & of course multi-threaded)
- Support for software screen grabbers: DirectX11, CoreGraphics, X11 :new:
- User interface upgraded to modern standards (Bootstrap 5) :new:
- Improved LUT table for SDR(yuv) and HDR video streams :new:
- Support for CEC (turn ON/OFF grabbers, remote keys to command HDR tone mapping)  :new:
- Support for my new [HyperSPI](https://github.com/awawa-dev/HyperSPI) project for Rpi. Fastest possible cable solution for almost every generic ESP8266/ESP32 LED driver :new:
- Fork of WLED with USB serial port AWA protocol at @2000000 speed (:sparkles: there are also special versions for @921600) for ESP32 & ESP8266 and almost all types of LED strips: [HyperSerialWLED](https://github.com/awawa-dev/HyperSerialWLED)
- Support for WS821x RGB, SK6812 RGBW, APA102 like LED strips using USB serial port AWA protocol for ESP8266 at @2000000 baud with data integrity check: [HyperSerialEsp8266](https://github.com/awawa-dev/HyperSerialEsp8266) :new:
- Support for WS821x RGB, SK6812 RGBW, APA102 like LED strips using USB serial port AWA protocol for ESP32 at @2000000 baud with data integrity check: [HyperSerialESP32](https://github.com/awawa-dev/HyperSerialESP32) :new:
- Automatic signal detection with smart learning capability for USB grabbers :new:
- Re-implemented backup import / export functions for ALL instances :new:
- New video stream crop method in JSON API and GET multi-command support :new:
- Auto-resume option for the USB grabber :new:
- JSON API documentation in a form of live playground in 'Advanced' tab :new:
- List of available COM ports for the adalight driver :new:
- Fix: in specific cases some devices could not react to the 'no video signal' event when it's triggered :new:
- New installer for Raspberry Pi 3 & 4 64bit OS (AARCH64), faster up to 30% over 32bit OS armv7l version :new:
- Fix for WLED new network protocol :new:
- LED grouping *aka* PC mode *aka* gradient mode, can help with eye fatigue when used with the monitor, each LED in the group has same average color :new:
- Add timeout for the anti-flickering filter :new:
- Panel for easy video resolution & refresh mode selection in the grabber section :new:
- Support for QT6 :new:
- Lower CPU usage when automatic signal detection triggers 'no-signal' :new:
- Fixed power saving issue in macOS version :new:
- Audio visualization effects (Windows, macOS and Linux)
- Support for YUV, MJPEG, RGB24, I420, NV12 and XRGB encoding
- Overall (_'Quarter of frame'_ in the USB grabber section) and per an instance (_'Sparse processing'_ in the _Processing_ tab) options to control quality/performance balance.
- Hardware brightness, contrast, saturation, hue control for USB grabbers (Windows and Linux)
- Philips Hue driver (inc. Entertainment API) partially rewritten and working. Customized new options for powering on/off the lamps
- New option to choose video encoding format (for multi format grabbers for ex. Ezcap 269, MS2109 clones)
- special LUT table dedicated for Ezcap 320 grabber available in the download section
- Add configurable Signal Threshold Counter option for signal detection
- LUT table tone mapping, mainly for HDR correction and fast color space transformation (YUV).
- New advanced & weighted advanced LED mean color algorithm in _Image&#8594;LED mapping_
- Improved backlight algorithm to minimize leds flickering on the dark scenes (configurable in the _Smoothing_)
- Add old style color calibration (HSL) using luminance, saturation et.
- Build for newer Raspbian Buster. It's a complete migration from older Raspbian Stretch
- Option for _hyperhdr-remote_, JSON API and web GUI remote to turn on/off HDR tone mapping
- Option for luminescence & saturation for hyperhdr-remote
- Ready to write SD images of HyperHDR
- Fix for SK9822 leds on SPI (aka fake APA102)
- Windows, macOS DMG and Linux DEB & RPM installers contain default LUT table

### Dedicated HDR LUT tables for USB grabbers

You can create your own calibrated LUT table using latest HyperHDR or use LUT dedicated for MS2109 clones, Navy U3, Ezcap 320, Ezcap 321, Ezcap 331, AV Access 4KVC00 (you can find them in the HyperHDR [release](https://github.com/awawa-dev/HyperHDR/releases/latest) section). Why is it worth using them? [USB grabbers HDR to SDR quality review](https://www.hyperhdr.eu/2022/04/usb-grabbers-hdr-to-sdr-quality-test.html)

### FAQ:

**You don't need to use an HDR source to use HyperHDR. You can just benefit from: high performance & optimized video proccessing, capability of using multithreading to avoid bottleneck resources, support for modern USB grabbers under Windows 10/Linux/macOS, decicated fast USB LED drivers (HyperSerialEsp8266, HyperSerialESP32 and HyperSerialWLED) or SPI driver (HyperSPI), screen grabbers including DirectX11 with HDR tone mapping, easy to use JSON API (both POST and GET), easy to setup automatic video signal detection, music capabilities and anti-flickering filter for best movie experience.**

Use linux 'top' command with per core view (press 1) or preferable 'htop'. On Rpi 2/3/4 max limit is 400% (4 cores per 100%). **The problem will occure when one of the core's usage is close to the 100% limit, not when overall usage is for example between 200-300% where each core if far from the individual limit.**

Check the performance statistics that are updated every minute in System➔Log page. 

Thanks to our colleague @mjoshd there is a HyperHDR integration plugin for Home Assistant. Check the details on the website of the project [https://github.com/mjoshd/hyperhdr-ha](https://github.com/mjoshd/hyperhdr-ha) Also you can test a HA plugin to install HyperHDR [link](https://github.com/ihrapsa/hassio-addons) (thanks @ihrapsa) 🚀 🆕    
  
We do not support driving WS281x and especially SK6812 LED strips directly from the Raspberry Pi although it's theoretically possible: [link](https://github.com/awawa-dev/HyperHDR/discussions/111). If you made it and it works, fine, but most of our users weren't so lucky. You should use external ESP8266/ESP32 (preferable with CH340G or CP2104 onboard) and the voltage level shifter. :warning:  

Usage of WS281x LED strip with Rpi directly (PWM mode) requires _root_ privilages. Otherwise you may get _'Error message: mmap() failed'_ ([read more](https://github.com/awawa-dev/HyperHDR/issues/52)) :warning:

It's possible to switch off/on HDR tone mapping remotely with your home automation system. You can build commands for HyperHDR using our JSON API playground :new:
  
  
**Before and after HyperHDR LUT correction on HDR/BT2020 video that was broken by the USB grabber.\
Without it your ambient lighting colors will be washed-out:**
![alt text](https://i.postimg.cc/VsbZrGBx/cfinal.jpg)
![alt text](https://i.postimg.cc/sXbnH7yH/afinal.jpg)
![alt text](https://i.postimg.cc/zDnSY9kG/dfinal.jpg)
![alt text](https://i.postimg.cc/nr73yrhF/bfinal.jpg)

## License
  
The source is released under MIT-License (see http://opensource.org/licenses/MIT).  
[![GitHub license](https://img.shields.io/badge/License-MIT-yellow.svg)](https://raw.githubusercontent.com/awawa-dev/HyperHDR/master/LICENSE)
