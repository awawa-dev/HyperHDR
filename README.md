## About 

Based on [Hyperion NG](https://github.com/hyperion-project/hyperion.ng) with some performance tweaks especially for MJPEG grabbers with support for HDR/BT2020 with LUT tables. 
Single and multi thread optimalization (Rpi2/3/4) for USB v4l2 grabber.

## Download packages & sources

https://github.com/awawa-dev/HyperHDR/releases

### New features of this fork:

* <b>Really low CPU</b> usage on SoCs like Raspberry Pi using v4l2 grabbers
* HDR/BT2020 color & gamma correction
* support for multithreading for USB grabbers

<b>Before and after on some HDR/BT2020 content that was broken by the video grabber:</b>
<img src='https://i.postimg.cc/SRdv0VFd/compare0.png'/>
<img src='https://i.postimg.cc/7PncTPGz/compare1.png'/>
<img src='https://i.postimg.cc/9FXkP3Zn/compare2.png'/>


<img src='https://i.postimg.cc/RZnQtydk/changes.png' border='0' alt='changes'/>

## License
The source is released under MIT-License (see http://opensource.org/licenses/MIT).<br>
[![GitHub license](https://img.shields.io/badge/License-MIT-yellow.svg)](https://raw.githubusercontent.com/hyperion-project/hyperion.ng/master/LICENSE)
