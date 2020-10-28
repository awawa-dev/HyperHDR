## About 

Based on [Hyperion NG](https://github.com/hyperion-project/hyperion.ng) with some performance tweaks especially for MJPEG/YUV grabbers with support for HDR/BT2020 using LUT tables.<br/>
Single and multi-threaded optimization (Rpi2/3/4) for USB v4l2 grabber.<br/>
Direct support for the devices under Windows 10.

## Support

https://hyperion-project.org/threads/sdr-hdr-1080p-4k-capable-setup-with-hyperion-ng-for-media-center.10652/

## Download packages & sources

https://github.com/awawa-dev/HyperHDR/releases

You can use prepared SD card images just like Hyperbian.<br/>
Default hostname for SD images is HyperHDR so connect to http://hyperhdr:8090/<br/>
SSH is enabled on default. Default LUT table is already included!

### New features of this fork:

* <b>Really low CPU</b> usage on SoCs like Raspberry Pi using v4l2 grabbers
* support for multithreading for USB grabbers
* HDR/BT2020 color & gamma correction
* support for Windows 10
* LUT table generator with import feature
* support for the old color calibration with luminance and saturation gain (classic Hyperion)
* Debian Buster with Qt5 targeted installers

### FAQ:

With HyperHDR you can see a jump of CPU usage in case of MJPEG encoding grabber to over 200-300% for 1080p format.
It's OK as MJPEG decoding is quite challenging. What is more important each core will be loaded at 50-60% only as you will go for full frame rate (1080p 30 or 60fps) on Rpi2/3/4 .
With single thread in Hyperion.NG CPU usage will be around 100% but it means that the framerate will be greatly reduced and components will be fight for resources.

Use linux 'top' command with per core view (press 1).

Enable logs with DEBUG mode to see the performance stats that are updated every minute.

Latest version 10.2.0.8 already includes default LUT table. But if you want use your own (for ex. brighter) then use Winscp for connecting to Raspbian to locate \home\pi\\.hyperion folder and for uploading lut_lin_tables.3d file. The folder is hidden for most FTP clients.

Use YUV encoding format if it's possible.

<b>Before and after on some HDR/BT2020 content that was broken by the video grabber:</b>
<img src='https://i.postimg.cc/SRdv0VFd/compare0.png'/>
<img src='https://i.postimg.cc/7PncTPGz/compare1.png'/>
<img src='https://i.postimg.cc/9FXkP3Zn/compare2.png'/>

<br/><br/><b>Direct support for USB grabbers under Windows 10:</b><br/>
<img src='https://i.postimg.cc/NG6NQkGb/p1.jpg'/>

<br/><br/><b>USB grabber configuration:</b><br/>
<img src='https://i.postimg.cc/yBZns4MG/s0.jpg'/>

<br/><br/><b>Example of color calibration:</b><br/>
<img src='https://i.postimg.cc/pR9g86nb/s1.jpg'/>

<br/><br/><b>Lut table generator:</b><br/>
<img src='https://i.postimg.cc/QDPS2xy5/s2.jpg'/>

## License
The source is released under MIT-License (see http://opensource.org/licenses/MIT).<br>
[![GitHub license](https://img.shields.io/badge/License-MIT-yellow.svg)](https://raw.githubusercontent.com/hyperion-project/hyperion.ng/master/LICENSE)
