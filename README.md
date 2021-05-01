## About 

Open source bias lighting implementation with the audio visualization effects and video performance tweaks especially for USB grabbers. Support for HDR/BT2020 using LUT tables. Single and multi-threaded video processing optimization on **Windows**, **macOS** and **Linux x64 & ARM** (Raspberry Pi and others) for SDR/HDR streams captured by USB grabbers. Now even Raspberry Pi 3 can handle 1080p60 MJPEG stream. Direct support for the devices under Windows 10 using Microsoft Media Foundation and under macOS using AVFoundation.

## Download packages & sources

https://github.com/awawa-dev/HyperHDR/releases

Default LUT table is already included (in the installers) but you can generate your own or import 3dl Autodesk lut table.\
For Raspberry Pi you may use prepared SD card images: [manual](https://hyperhdr.blogspot.com/2020/11/hyperhdr-prepare-for-building-buying_17.html)\
Default hostname for SD images is HyperHDR so connect to http://hyperhdr:8090/ \
SSH and SPI are enabled on default.

## How to compile

[Building HyperHDR from sources](https://github.com/awawa-dev/HyperHDR/blob/master/CompileHowto.md) :sparkles:

## Support and contact

[HyperHDR's discussion section](https://github.com/awawa-dev/HyperHDR/discussions) (or https://hyperhdr.blogspot.com/ )

**Manuals and guides for building and configuring your own bias lighting system with HyperHDR:**

[Step 1: Addressable led strip](https://hyperhdr.blogspot.com/2020/11/blog-post.html)\
[Step 2: Power supply](https://hyperhdr.blogspot.com/2020/11/hyperhdr-prepare-for-building-buying.html)\
[Step 3: USB grabber](https://hyperhdr.blogspot.com/2020/11/hyperhdr-prepare-for-building-buying_12.html)\
[Step 4: Additional stuff](https://hyperhdr.blogspot.com/2020/11/hyperhdr-prepare-for-building-buying_13.html)\
[Step 5: Device for hosting HyperHDR...macOS, Windows or Raspberry Pi?](https://hyperhdr.blogspot.com/2020/11/hyperhdr-prepare-for-building-buying_14.html) :sparkles: \
[Step 6: Installing HyperHDR bias lighting software](https://hyperhdr.blogspot.com/2020/11/hyperhdr-prepare-for-building-buying_17.html) :sparkles: \
[Step 7: How to set up HyperHDR? Part I: basic configuration](https://hyperhdr.blogspot.com/2021/04/how-to-set-up-hyperhdr-part-i-basic.html) :new:

[Build-log from my SK6812 RGBW system and one thing about calibration](https://hyperhdr.blogspot.com/2020/12/my-build-log-using-sk6812-rgbw-led.html)

### Main features of HyperHDR fork:

* <b>Really low CPU</b> usage on SoCs like Raspberry Pi using v4l2 grabbers
* Support for multithreading that makes Raspberry Pi capable of processing HQ video stream (Rpi 1 & Zero should also benefit from the optimization alone)
* Built-in LUT table generator at: ````http://<IP OF YOUR HYPERHDR>:8090/content/lut_generator.html```` :sparkles:
* HDR/BT2020 color, treshold & gamma correction (LUT)
* SDR treshold & gamma correction for selected codecs (LUT)
* Support for USB grabbers under Windows 10
* Support for USB grabbers under macOS (x64) :new:
* Built-in audio visualization effects
* Optimized multi-instances. You can use for example your TV LED strip and multiple WLED or Philips Hue light sources.
* Support for WS8212b/WS8213 RGB & SK6812RGBW ultrafast USB serial port AWA protocol for ESP8266 at @2000000 baud with data integrity check and white channel calibration: [HyperSerialEsp8266](https://github.com/awawa-dev/HyperSerialEsp8266). WLED fork for ESP8266 & ESP32 at @2000000 baud and almost all popular types of LED strips is also available: [HyperSerialWLED](https://github.com/awawa-dev/HyperSerialWLED)

##
<b>Changelog:</b>
- Overall performance without tone mapping for USB grabbers improved x10 (MJPEG) and x3 (YUV) over Hyperion NG 2.0.0.8A thanks to optimization & using of multi-threading
- Direct support for USB grabbers under Windows 10, Linux and macOS (really fast & of course multi-threaded)
- Audio visualization effects (Windows, macOS and Linux)
- Fork of WLED with USB serial port AWA protocol at @2000000 for ESP32 & ESP8266 and almost all types of LED: [HyperSerialWLED](https://github.com/awawa-dev/HyperSerialWLED)
- Support for WS8212b/WS8213 RGB & SK6812 RGBW USB serial port AWA protocol for ESP8266 at @2000000 baud with data integrity check: [HyperSerialEsp8266](https://github.com/awawa-dev/HyperSerialEsp8266)
- Support for YUV, MJPEG, RGB24, I420, NV12 and XRGB encoding
- Overall (_'Quarter of frame'_ in the USB grabber section) and per an instance (_'Sparse processing'_ in the _Processing_ tab) options to control quality/performance balance.
- Hardware brightness, contrast, saturation, hue control for USB grabbers (Windows and Linux)
- Philips Hue driver (inc. Entertainment API) partially rewritten and working. Customized new options for powering on/off the lamps
- New option to choose video encoding format (for multi format grabbers for ex. Ezcap 269, MS2109 clones)
- special LUT table dedicated for Ezcap 320 grabber available in the download section
- Add configurable Signal Threshold Counter option for signal detection
- LUT table tone mapping, mainly for HDR correction and fast color space transformation (YUV).
- New advanced & weighted advanced LED mean color algorithm in _Image&#8594;LED mapping_
- Improved backlight algorithm to minimize leds flickering on the dark scenes (configurable in the _Smoothing_) :new:
- Add old style color calibration (HSL) using luminance, saturation et.
- Build for newer Raspbian Buster. It's a complete migration from older Raspbian Stretch
- Option for _hyperhdr-remote_, JSON API and web GUI remote to turn on/off HDR tone mapping
- Option for luminescence & saturation for hyperhdr-remote
- Ready to write SD images of HyperHDR
- Fix for SK9822 leds on SPI (aka fake APA102)
- Windows, macOS DMG and Linux DEB & RPM installers contain default LUT table

### FAQ:

**You don't need to use HDR source for usage of HyperHDR. You can just benefit from significant higher performance for captured video stream from USB grabber over Hyperion NG,  capability of using multithreading to avoid bottleneck resources, support for modern USB grabbers under Windows 10/Linux/macOS, decicated fast USB LED drivers (HyperSerialEsp8266 and HyperSerialWLED), improved image to led colors averaging new algorihtms, music capabilities and few tweaks that minimize blinking at the dark scenes.**

With HyperHDR you can see a jump of CPU usage in case of MJPEG encoding grabber to over 200-300% for 1080p format for Rpi 2/3/4.
It's OK as MJPEG decoding is quite challenging. What is more important each core will be loaded at 50-60% only as you will go for full frame rate (1080p 30 or 60fps).
With a single thread in Hyperion NG CPU usage will be around 100%, but it means that the framerate will be greatly reduced and components will be fight for resources.

Use linux 'top' command with per core view (press 1) or preferable 'htop'. On Rpi 2/3/4 max limit is 400% (4 cores per 100%). **The problem will occure when one of the core's usage is close to the 100% limit, not when overall usage is for example between 200-300% where each core if far from the individual limit.**

Check the performance statistics that are updated every minute in System➔Log page.

Use YUV(YUY2) / XRGB(RGB32) encoding format if it's possible. It provides better quality and lower CPU usage.

Usage of WS281x LED strip with Rpi directly (PWM mode) requires _root_ privilages. Otherwise you may get _'Error message: mmap() failed'_ ([read more](https://github.com/awawa-dev/HyperHDR/issues/52)) :warning:

It's possible to switch off/on HDR tone mapping remotely with home automation system. Home Assistant is recommended ([read more](https://github.com/awawa-dev/HyperHDR/issues/56#issuecomment-822017134)) :sparkles:

\
\
**Before and after HyperHDR LUT correction on HDR/BT2020 video that was broken by the USB grabber.\
Without it your bias lighting colors will be washed-out:**
![alt text](https://i.postimg.cc/VsbZrGBx/cfinal.jpg)
![alt text](https://i.postimg.cc/sXbnH7yH/afinal.jpg)
![alt text](https://i.postimg.cc/zDnSY9kG/dfinal.jpg)
![alt text](https://i.postimg.cc/nr73yrhF/bfinal.jpg)

**Direct support for USB grabbers under Windows 10 using Microsoft Media Foundation:**
![alt text](https://i.postimg.cc/DfwF9bsj/win10.jpg)

**Direct support for USB grabbers under macOS using AVFoundation:**
![alt text](https://i.postimg.cc/6Bg4GKnY/Screen-Shot-2021-04-25-at-11-49-32-PM.png)

**Advanced LUT table generator for SDR & HDR:**
![LUT generator](https://i.postimg.cc/LmP6PYxy/LUT-table.jpg)

## License
The source is released under MIT-License (see http://opensource.org/licenses/MIT).<br>
[![GitHub license](https://img.shields.io/badge/License-MIT-yellow.svg)](https://raw.githubusercontent.com/awawa-dev/HyperHDR/master/LICENSE)
