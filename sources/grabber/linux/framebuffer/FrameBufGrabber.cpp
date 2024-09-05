/* FrameBufGrabber.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2024 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/HyperHDR
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
 */

#include <iostream>
#include <sys/ioctl.h>
#include <stdexcept>
#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <sys/mman.h>
#include <linux/fb.h>

#include <base/HyperHdrInstance.h>
#include <base/AccessManager.h>

#include <QDirIterator>
#include <QFileInfo>
#include <QCoreApplication>

#include <grabber/linux/framebuffer/FrameBufGrabber.h>

FrameBufGrabber::FrameBufGrabber(const QString& device, const QString& configurationPath)
	: Grabber(configurationPath, "FRAMEBUFFER_SYSTEM:" + device.left(14))
	, _configurationPath(configurationPath)
	, _semaphore(1)
	, _handle(-1)
{
	_timer.setTimerType(Qt::PreciseTimer);
	connect(&_timer, &QTimer::timeout, this, &FrameBufGrabber::grabFrame);

	getDevices();
}

QString FrameBufGrabber::GetSharedLut()
{
	return "";
}

void FrameBufGrabber::loadLutFile(PixelFormat color)
{		
}

void FrameBufGrabber::setHdrToneMappingEnabled(int mode)
{
}

FrameBufGrabber::~FrameBufGrabber()
{
	uninit();
}

void FrameBufGrabber::uninit()
{
	// stop if the grabber was not stopped
	if (_initialized)
	{		
		stop();
		Debug(_log, "Uninit grabber: %s", QSTRING_CSTR(_deviceName));
	}
	
	_initialized = false;
}

bool FrameBufGrabber::init()
{
	Debug(_log, "init");


	if (!_initialized)
	{
		QString foundDevice = "";
		bool    autoDiscovery = (QString::compare(_deviceName, Grabber::AUTO_SETTING, Qt::CaseInsensitive) == 0);

		enumerateDevices(true);


		if (!autoDiscovery && !_deviceProperties.contains(_deviceName))
		{
			Debug(_log, "Device %s is not available. Changing to auto.", QSTRING_CSTR(_deviceName));
			autoDiscovery = true;
		}

		if (autoDiscovery)
		{
			Debug(_log, "Forcing auto discovery device");
			if (!_deviceProperties.isEmpty())
			{				
				foundDevice = _deviceProperties.firstKey();
				_deviceName = foundDevice;
				Debug(_log, "Auto discovery set to %s", QSTRING_CSTR(_deviceName));
			}
		}
		else
			foundDevice = _deviceName;

		if (foundDevice.isNull() || foundDevice.isEmpty() || !_deviceProperties.contains(foundDevice))
		{
			Error(_log, "Could not find any capture device.");
			return false;
		}

		
		Info(_log, "*************************************************************************************************");
		Info(_log, "Starting FrameBuffer grabber. Selected: '%s' (%i) max width: %d (%d) @ %d fps", QSTRING_CSTR(foundDevice), _deviceProperties[foundDevice].valid.first().input, _width, _height, _fps);
		Info(_log, "*************************************************************************************************");		

		_handle = open(QSTRING_CSTR(foundDevice), O_RDONLY);
		if (_handle < 0)
		{
			Error(_log, "Could not open the framebuffer device: '%s'. Reason: %s (%i)", QSTRING_CSTR(foundDevice), std::strerror(errno), errno);			
		}
		else
		{
			struct fb_var_screeninfo scr;

			if (ioctl(_handle, FBIOGET_VSCREENINFO, &scr) == 0)
			{
				if (scr.bits_per_pixel == 16 || scr.bits_per_pixel == 24 || scr.bits_per_pixel == 32)
				{
					_actualDeviceName = foundDevice;
					Info(_log, "Device '%s' is using currently %ix%ix%i resolution.", QSTRING_CSTR(_actualDeviceName), scr.xres, scr.yres, scr.bits_per_pixel);
					_initialized = true;
				}
				else
				{
					Error(_log, "Unsupported %ix%ix%i mode for '%s' device.", scr.xres, scr.yres, scr.bits_per_pixel, QSTRING_CSTR(foundDevice));
					close(_handle);
					_handle = -1;
				}
			}
			else
			{
				Error(_log, "Could not get the framebuffer dimension for '%s' device. Reason: %s (%i)", QSTRING_CSTR(foundDevice), std::strerror(errno), errno);
				close(_handle);
				_handle = -1;
			}			
		}
	}

	return _initialized;
}


void FrameBufGrabber::getDevices()
{
	enumerateDevices(false);
}

bool FrameBufGrabber::isActivated()
{
	return !_deviceProperties.isEmpty();
}

void FrameBufGrabber::enumerateDevices(bool silent)
{
	_deviceProperties.clear();

	for (int i = 0; i <= 16; i++)
	{
		QString path = QString("/dev/fb%1").arg(i);
		if (QFileInfo(path).exists())
		{
			DeviceProperties properties;
			DevicePropertiesItem dpi;

			dpi.input = i;
			properties.valid.append(dpi);

			_deviceProperties.insert(path, properties);

			if (!silent)
				Info(_log, "Found FrameBuffer device: %s", QSTRING_CSTR(path));
		}
	}	
}

bool FrameBufGrabber::start()
{
	try
	{		
		if (init())
		{
			_timer.setInterval(1000/_fps);
			_timer.start();
			Info(_log, "Started");
			return true;
		}
	}
	catch (std::exception& e)
	{
		Error(_log, "start failed (%s)", e.what());
	}

	return false;
}

void FrameBufGrabber::stop()
{
	if (_initialized)
	{
		_semaphore.acquire();
		_timer.stop();

		if (_handle >= 0)
		{
			close(_handle);
			_handle = -1;
		}
		_initialized = false;
		
		_semaphore.release();
		Info(_log, "Stopped");
	}
}

void FrameBufGrabber::grabFrame()
{
	bool stopNow = false;
	
	if (_semaphore.tryAcquire())
	{
		if (_initialized)
		{
			/// GETFRAME
			struct fb_var_screeninfo scr;
			bool isStillActive = false;

			if (ioctl(_handle, FBIOGET_VSCREENINFO, &scr) == 0)
			{
				isStillActive = true;
			}
			else
			{
				Warning(_log, "The handle is lost. Trying to restart the driver.");

				close(_handle);
				_handle = open(QSTRING_CSTR(_actualDeviceName), O_RDONLY);

				if (_handle >= 0 && ioctl(_handle, FBIOGET_VSCREENINFO, &scr) == 0)
				{
					isStillActive = true;
				}
			}

			if (isStillActive)
			{
				_actualWidth = scr.xres;
				_actualHeight = scr.yres;

				if (scr.bits_per_pixel == 16 || scr.bits_per_pixel == 24 || scr.bits_per_pixel == 32)
				{
					struct fb_fix_screeninfo format;

					if (ioctl(_handle, FBIOGET_FSCREENINFO, &format) >= 0)
					{
						uint8_t* memHandle = static_cast<uint8_t*>(mmap(nullptr, format.smem_len, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, _handle, 0));

						if (memHandle == MAP_FAILED)
						{
							Error(_log, "Could not map the framebuffer memory.");
							stopNow = true;
						}
						else
						{
							if (scr.bits_per_pixel == 32)
								processSystemFrameBGRA(memHandle, format.line_length);
							else if (scr.bits_per_pixel == 24)
								processSystemFrameBGR(memHandle, format.line_length);
							else if (scr.bits_per_pixel == 16)
								processSystemFrameBGR16(memHandle, format.line_length);

							munmap(memHandle, format.smem_len);
						}						
					}
					else
					{
						Error(_log, "Could not read the framebuffer properties.");
						stopNow = true;
					}
				}
			}
			else
			{
				Error(_log, "Could not read the framebuffer dimension.");
				stopNow = true;
			}
		}
		_semaphore.release();
	}

	if (stopNow)
	{
		uninit();
	}
}


void FrameBufGrabber::setCropping(unsigned cropLeft, unsigned cropRight, unsigned cropTop, unsigned cropBottom)
{
	_cropLeft = cropLeft;
	_cropRight = cropRight;
	_cropTop = cropTop;
	_cropBottom = cropBottom;
}
