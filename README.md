## About 

Based on [Hyperion NG](https://github.com/hyperion-project/hyperion.ng) with some performance tweaks especially for MJPEG/YUV grabbers with support for HDR/BT2020 using LUT tables.<br/>
Single and multi-threaded optimization (Rpi2/3/4) for USB v4l2 grabber.

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
* LUT table generator with import feature
* support for the old color calibration with luminance and saturation gain (classic Hyperion)

### FAQ:

With HyperHDR you can see a jump of CPU usage in case of MJPEG encoding grabber to over 200-300% for 1080p format.
It's OK as MJPEG decoding is quite challenging. What is more important each core will be loaded at 50-60% only as you will go for full frame rate (1080p 30 or 60fps) on Rpi2/3/4.
With single thread in Hyperion.NG CPU usage will be around 100% but it means that the framerate will be greatly reduced and components will be fight for resources.

Use linux 'top' command with per core view (press 1).

Enable logs with DEBUG mode to see the performance stats that are updated every minute.

Use Winscp for connecting to Raspbian to locate \home\pi\\.hyperion folder and upload lut_lin_tables.3d file.
The folder is hidden for most FTP clients.

Use YUV encoding format if it's possible.

Enabling backlight can produce flickering in dark scenes as always it does. That's because pixels from frame's sequence can balance on the threshold level. Use coloured backlight threshold on the lowest setting and enable smoothing with maximum continues ouput at 100hz.

<b>Before and after on some HDR/BT2020 content that was broken by the video grabber:</b>
<img src='https://i.postimg.cc/SRdv0VFd/compare0.png'/>
<img src='https://i.postimg.cc/7PncTPGz/compare1.png'/>
<img src='https://i.postimg.cc/9FXkP3Zn/compare2.png'/>

<br/><b>Configuration:</b>
<img src='https://i.postimg.cc/prymCNLj/screenshot8.png'/>

<br/><b>Lut table generator:</b>
<img src='https://i.postimg.cc/mgVhv219/lutgenerator.png'/>

## License
The source is released under MIT-License (see http://opensource.org/licenses/MIT).<br>
[![GitHub license](https://img.shields.io/badge/License-MIT-yellow.svg)](https://raw.githubusercontent.com/hyperion-project/hyperion.ng/master/LICENSE)
