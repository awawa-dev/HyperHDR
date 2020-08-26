## About 

This fork is based on [Hyperion](https://github.com/hyperion-project/hyperion.ng) that is an opensource [Bias or Ambient Lighting](https://en.wikipedia.org/wiki/Bias_lighting) implementation which you might know from TV manufacturers. It supports many LED devices and video grabbers. The project is still in a alpha development stage (no stable release available).

## Download packages & sources

https://github.com/awawa-dev/hyperion.ng/releases

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
