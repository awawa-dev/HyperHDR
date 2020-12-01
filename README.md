## About 

Ambilight software system with some performance tweaks especially for MJPEG/YUV grabbers with support for HDR/BT2020 using LUT tables.<br/>Single and multi-threaded optimization (Windows PC and RPi 2/3/4) for SDR/HDR streams captured by USB grabbers. Now even Rpi 3 can handle 1080p60 MJPEG stream.<br/>Direct support for the devices under Windows 10 using Media Foundation.

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

### Main features of this fork:

* <b>Really low CPU</b> usage on SoCs like Raspberry Pi using v4l2 grabbers
* support for multithreading
* HDR/BT2020 color & gamma correction
* support for USB grabbers under Windows 10
* LUT table generator with import feature
* support for the old color calibration with luminance and saturation gain (classic Hyperion)
* Debian Buster with Qt5 targeted installers

##
<b>Changelog:</b>
- Overall performance without tone mapping for USB grabbers improved x10 (MJPEG) and x3 (YUV) over Hyperion NG 2.0.0.8A thanks to optimization & using of multi-threading
- Direct support for USB grabbers under Windows 10 using Microsoft Media Foundation (really fast & of course multi-threaded)
- Build for newer Raspbian Buster. It's a complete migration from older Raspbian Stretch.
- Option for hyperion-remote, JSON API and web GUI remote to turn on/off HDR tone mapping
- MJPEG & YUV HDR LUT tone mapping
- Hardware brightness, contrast, saturation, hue control for USB grabbers (both Windows and Linux)
- Ready to write SD images of HyperHDR
- New option to choose video encoding format (for multi format grabbers for ex. Ezcap 269, MS2109 clones).
- Add configurable Signal Threshold Counter option for signal detection
- Option for luminescence & saturation for hyperion-remote
- New advanced LED mean color algorithm in image->LED mapping
- New weighted advanced LED mean color algorithm in image->LED mapping
- Improved backlight algorithm to minimize leds flickering on the dark scenes (smoothing is still recommended)
- Add old style color calibration (from Hyperion classic) using luminance, saturation et.
- Fix for SK9822 leds on SPI (aka fake APA102)
- Required libglvnd library dependency included for tar container.
- Improved YUV decoding using LUT tables for speed up
- Windows installer contains default LUT table
- Installers for DEB & RPM now include LUT table 

### FAQ:

<b>You don't need to use HDR source for usage of HyperHDR. You can just benefit from significant higher performance for captured video stream from USB grabber over Hyperion NG,  capability of using multithreading to avoid bottleneck resources, support for modern USB grabbers under Windows 10, improved image to led colors averaging new algorihtms, few tweaks that minimize blinking at the dark scenes and hardware support to tune up brightness, contrast, saturation and hue for a grabber.</b>

The default settings in the LED color calibration are made for my setup, currently WS2801 ;) It's recommended for you to create your own.

Try to play with grabber's hardware brightness and contrast as experimentents proved they can change even with some selected resolutions and FPS. And there are some different default values for brightness and contrast for Linux and Windows. Nothing is constant if you don't force specific value. For Ezcap 269 grabber I set brightness to 139 and preferable resolution is 1280x720 YUV.

With HyperHDR you can see a jump of CPU usage in case of MJPEG encoding grabber to over 200-300% for 1080p format for Rpi 2/3/4.
It's OK as MJPEG decoding is quite challenging. What is more important each core will be loaded at 50-60% only as you will go for full frame rate (1080p 30 or 60fps).
With a single thread in Hyperion NG CPU usage will be around 100%, but it means that the framerate will be greatly reduced and components will be fight for resources.

Use linux 'top' command with per core view (press 1) or preferable 'htop'. On Rpi 2/3/4 max limit is 400% (4 cores per 100%).

Check the performance statistics that are updated every minute in System->Log page.

Use YUV encoding format if it's possible. It provides better quality and lower CPU usage.

<br/><b>Before and after on some HDR/BT2020 content that was broken by the video grabber:</b>
<img src='https://i.postimg.cc/VsbZrGBx/cfinal.jpg'/>
<img src='https://i.postimg.cc/sXbnH7yH/afinal.jpg'/>
<img src='https://i.postimg.cc/zDnSY9kG/dfinal.jpg'/>
<img src='https://i.postimg.cc/nr73yrhF/bfinal.jpg'/>

<br/><br/><b>Direct support for USB grabbers under Windows 10:</b><br/>
<img src='https://i.postimg.cc/DfwF9bsj/win10.jpg'/>

<br/><br/><b>USB grabber configuration:</b><br/>
<img src='https://i.postimg.cc/44Jnnqgd/interface.png'/>

## License
The source is released under MIT-License (see http://opensource.org/licenses/MIT).<br>
[![GitHub license](https://img.shields.io/badge/License-MIT-yellow.svg)](https://raw.githubusercontent.com/hyperion-project/hyperion.ng/master/LICENSE)
