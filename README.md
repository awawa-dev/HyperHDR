## About 

Open source ambilight implementation with the audio visualization effects and video performance tweaks especially for USB grabbers. Support for HDR/BT2020 using LUT tables. Single and multi-threaded optimization (Windows PC and RPi 2/3/4) for SDR/HDR streams captured by USB grabbers. Now even Rpi 3 can handle 1080p60 MJPEG stream. Direct support for the devices under Windows 10 using Media Foundation.

## Download packages & sources

https://github.com/awawa-dev/HyperHDR/releases

Default LUT table is already included (in the installers) but you can generate your own or import 3dl Autodesk lut table.<br/>
You can use prepared SD card images: [manual](https://hyperhdr.blogspot.com/2020/11/hyperhdr-prepare-for-building-buying_17.html)<br/>
Default hostname for SD images is HyperHDR so connect to http://hyperhdr:8090/<br/>
SSH and SPI are enabled on default. 

## Support and contact

https://hyperhdr.blogspot.com/

<br/><b>Guide for building your own ambilight system:</b><br/><br/>
<a href="https://hyperhdr.blogspot.com/2020/11/blog-post.html">Step 1: Addressable led strip</a><br/>
<a href="https://hyperhdr.blogspot.com/2020/11/hyperhdr-prepare-for-building-buying.html">Step 2: Power supply</a><br/>
<a href="https://hyperhdr.blogspot.com/2020/11/hyperhdr-prepare-for-building-buying_12.html">Step 3: USB grabber</a><br/>
<a href="https://hyperhdr.blogspot.com/2020/11/hyperhdr-prepare-for-building-buying_13.html">Step 4: Additional stuff</a><br/>
<a href="https://hyperhdr.blogspot.com/2020/11/hyperhdr-prepare-for-building-buying_14.html">Step 5: Device for hosting HyperHDR...Windows PC or Raspberry Pi?</a><br/>
<a href="https://hyperhdr.blogspot.com/2020/11/hyperhdr-prepare-for-building-buying_17.html">Step 6: Installing HyperHDR ambilight software</a><br/>
<br/>
<a href="https://hyperhdr.blogspot.com/2020/12/my-build-log-using-sk6812-rgbw-led.html">Build-log from my SK6812 RGBW system and one thing about calibration</a><br/>

### Main features of this fork:

* <b>Really low CPU</b> usage on SoCs like Raspberry Pi using v4l2 grabbers
* Support for multithreading that makes Rpi 2/3/4 capable of processing HQ video stream (Rpi Zero should also benefit from the optimization alone)
* HDR/BT2020 color & gamma correction
* Support for USB grabbers under Windows 10
* Built-in audio visualization effects
* Optimized multi-instances handling. You can use for example your TV LED strip alongside multiple WLED or Philips Hue light sources.
* Support for WS8212b/WS8213 RGB & SK6812RGBW ultrafast USB serial port AWA protocol for ESP8266 at 2000000 baud with data integrity check and white channel calibration: <a href="https://github.com/awawa-dev/HyperSerialEsp8266">HyperSerialEsp8266</a>. WLED fork is also available: <a href="https://github.com/awawa-dev/HyperSerialWLED">HyperSerialWLED</a>

##
<b>Changelog:</b>
- Overall performance without tone mapping for USB grabbers improved x10 (MJPEG) and x3 (YUV) over Hyperion NG 2.0.0.8A thanks to optimization & using of multi-threading
- Direct support for USB grabbers under Windows 10 using Microsoft Media Foundation (really fast & of course multi-threaded)
- Audio visualization effects (Windows and Linux). Just setup your audio grabber in the <i>effects</i> tabs and run audio effects in the <i>remote</i> tab.
- Support for WS8212b/WS8213 RGB & SK6812 RGBW ultrafast USB serial port AWA protocol for ESP8266 at 2 000 000 baud with data integrity check: <a href="https://github.com/awawa-dev/HyperSerialEsp8266">HyperSerialEsp8266</a>
- Fork of WLED with the support for USB serial port AWA protocol at 2 000 000 baud: <a href="https://github.com/awawa-dev/HyperSerialWLED">HyperSerialWLED</a>
- Support for I420, NV12 and XRGB encoding
- Overall ('Quarter of frame' in the USB grabber section) and per an instance ('Sparse processing' in the processing tab) options to control quality/performance balance.
- Hardware brightness, contrast, saturation, hue control for USB grabbers (both Windows and Linux)
- Philips Hue driver (inc. Entertainment API) partially rewritten and working. Customized new options for powering on/off the lamps.
- New option to choose video encoding format (for multi format grabbers for ex. Ezcap 269, MS2109 clones)
- Add configurable Signal Threshold Counter option for signal detection
- LUT table tone mapping, mainly for HDR correction and fast color space transformation (YUV).
- New advanced LED mean color algorithm in image->LED mapping
- New weighted advanced LED mean color algorithm in image->LED mapping
- Improved backlight algorithm to minimize leds flickering on the dark scenes (smoothing is still recommended)
- Add old style color calibration (HSL) using luminance, saturation et.
- Build for newer Raspbian Buster. It's a complete migration from older Raspbian Stretch.
- Option for hyperhdr-remote, JSON API and web GUI remote to turn on/off HDR tone mapping
- Option for luminescence & saturation for hyperhdr-remote
- Ready to write SD images of HyperHDR
- Fix for SK9822 leds on SPI (aka fake APA102)
- Windows and DEB & RPM installers contain default LUT table

### FAQ:

<b>You don't need to use HDR source for usage of HyperHDR. You can just benefit from significant higher performance for captured video stream from USB grabber over Hyperion NG,  capability of using multithreading to avoid bottleneck resources, support for modern USB grabbers under Windows 10, improved image to led colors averaging new algorihtms, few tweaks that minimize blinking at the dark scenes and hardware support to tune up brightness, contrast, saturation and hue for a grabber.</b>

The default settings in the LED color calibration are made for my setup, currently WS2801 ;) It's recommended for you to create your own.

Try to play with grabber's hardware brightness and contrast as experimentents proved they can change even with some selected resolutions and FPS. And there are some different default values for brightness and contrast for Linux and Windows. Nothing is constant if you don't force specific value. For Ezcap 269 grabber I set brightness to 139 and preferable resolution is 1280x720 YUV.

With HyperHDR you can see a jump of CPU usage in case of MJPEG encoding grabber to over 200-300% for 1080p format for Rpi 2/3/4.
It's OK as MJPEG decoding is quite challenging. What is more important each core will be loaded at 50-60% only as you will go for full frame rate (1080p 30 or 60fps).
With a single thread in Hyperion NG CPU usage will be around 100%, but it means that the framerate will be greatly reduced and components will be fight for resources.

Use linux 'top' command with per core view (press 1) or preferable 'htop'. On Rpi 2/3/4 max limit is 400% (4 cores per 100%). <b>The problem will occure when one of the core's usage is close to the 100% limit, not when overall usage is for example between 200-300% where each core if far from the individual limit.</b>

Check the performance statistics that are updated every minute in System->Log page.

Use YUV(YUY2) / XRGB(RGB32) encoding format if it's possible. It provides better quality and lower CPU usage.

<br/>
<br/>
<b>Before and after HyperHDR LUT correction on some HDR/BT2020 video content that was broken by the USB grabber.<br/>Without it your ambilight colors will be washed-out:<br/></b>
<img src='https://i.postimg.cc/VsbZrGBx/cfinal.jpg'/>
<img src='https://i.postimg.cc/sXbnH7yH/afinal.jpg'/>
<img src='https://i.postimg.cc/zDnSY9kG/dfinal.jpg'/>
<img src='https://i.postimg.cc/nr73yrhF/bfinal.jpg'/>

<br/><br/><b>Direct support for USB grabbers under Windows 10:</b><br/>
<img src='https://i.postimg.cc/DfwF9bsj/win10.jpg'/>

<br/><br/><b>USB grabber configuration:</b><br/>
<img src='https://i.postimg.cc/9cbKk0N9/newscreen5.png'/>

<br/><br/><b>Ultrafast USB serial port AWA protocol for ESP8266 at 2 000 000 baud:</b><br/>
<p align="center"><img src="https://i.postimg.cc/7h0KZxnn/usage.jpg"/></p>

## License
The source is released under MIT-License (see http://opensource.org/licenses/MIT).<br>
[![GitHub license](https://img.shields.io/badge/License-MIT-yellow.svg)](https://raw.githubusercontent.com/awawa-dev/HyperHDR/master/LICENSE)
