/* X11Grabber.cpp
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
#include <sstream>
#include <stdexcept>
#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>

#include <base/HyperHdrInstance.h>

#include <QDirIterator>
#include <QFileInfo>
#include <QCoreApplication>

#include <grabber/linux/X11/X11Grabber.h>
#include <grabber/linux/X11/smartX11.h>
#include <dlfcn.h>

struct x11Displays* (*_enumerateX11Displays)() = nullptr;
void (*_releaseX11Displays)(struct x11Displays* buffer) = nullptr;
x11Handle* (*_initX11Display)(int display) = nullptr;
void (*_uninitX11Display)(x11Handle* retVal) = nullptr;
unsigned char* (*_getFrame)(x11Handle * retVal) = nullptr;
void (*_releaseFrame)(x11Handle* retVal) = nullptr;

X11Grabber::X11Grabber(const QString& device, const QString& configurationPath)
	: Grabber(configurationPath, "X11_SYSTEM:" + device.left(14))
	, _configurationPath(configurationPath)
	, _semaphore(1)
	, _library(nullptr)
	, _actualDisplay(0)
	, _handle(nullptr)
{
	_timer.setTimerType(Qt::PreciseTimer);
	connect(&_timer, &QTimer::timeout, this, &X11Grabber::grabFrame);

	// Load library
	_library = dlopen("libsmart-x11.so", RTLD_NOW);

	if (_library)
	{
		_enumerateX11Displays = (x11Displays * (*)()) dlsym(_library, "enumerateX11Displays");
		_releaseX11Displays = (void (*)(x11Displays*)) dlsym(_library, "releaseX11Displays");
		_initX11Display = (x11Handle * (*)(int)) dlsym(_library, "initX11Display");
		_uninitX11Display = (void (*)(x11Handle*)) dlsym(_library, "uninitX11Display");
		_getFrame = (unsigned char* (*)(x11Handle*)) dlsym(_library, "getFrame");
		_releaseFrame = (void (*)(x11Handle*)) dlsym(_library, "releaseFrame");
	}
	else
		Error(_log, "Could not load X11 proxy library. Did you install libx11? Error: %s", dlerror());

	if  (_library && (_enumerateX11Displays == nullptr || _releaseX11Displays == nullptr || _releaseFrame == nullptr ||
		_initX11Display == nullptr || _uninitX11Display == nullptr || _getFrame == nullptr))
	{		
		Error(_log, "Could not load X11 proxy library definition. Did you install libx11? Error: %s", dlerror());

		dlclose(_library);
		_library = nullptr;
	}
	
	// Refresh devices
	if (_library)
	{
		Info(_log, "Loaded X11 proxy library for screen capturing");
		getDevices();
	}	
}

QString X11Grabber::GetSharedLut()
{
	return "";
}

void X11Grabber::loadLutFile(PixelFormat color)
{		
}

void X11Grabber::setHdrToneMappingEnabled(int mode)
{
}

X11Grabber::~X11Grabber()
{
	uninit();

	// destroy core elements from contructor
	if (_handle != nullptr)
	{
		_uninitX11Display(_handle);
		_handle = nullptr;
	}

	if (_library)
	{
		dlclose(_library);
		_library = nullptr;
	}
}

void X11Grabber::uninit()
{
	// stop if the grabber was not stopped
	if (_initialized)
	{		
		stop();
		Debug(_log, "Uninit grabber: %s", QSTRING_CSTR(_deviceName));
	}
	

	_initialized = false;
}

bool X11Grabber::init()
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
			Error(_log, "Could not find any capture device. X11 system grabber won't work if HyperHDR is run as a service due to security restrictions.");
			return false;
		}

		
		Info(_log, "*************************************************************************************************");
		Info(_log, "Starting X11 grabber. Selected: '%s' (%i) max width: %d (%d) @ %d fps", QSTRING_CSTR(foundDevice), _deviceProperties[foundDevice].valid.first().input, _width, _height, _fps);
		Info(_log, "*************************************************************************************************");		

		if (init_device(_deviceProperties[foundDevice].valid.first().input))
			_initialized = true;		
	}

	return _initialized;
}


void X11Grabber::getDevices()
{
	enumerateDevices(false);
}

bool X11Grabber::isActivated()
{
	return !_deviceProperties.isEmpty();
}

void X11Grabber::enumerateDevices(bool silent)
{
	_deviceProperties.clear();

	if (_library != nullptr)
	{
		x11Displays* list = _enumerateX11Displays();

		if (list != nullptr)
		{
			for (int i = 0; i < list->count; i++)
			{
				DeviceProperties properties;
				DevicePropertiesItem dpi;

				QString id = "X11: " + QString::fromLocal8Bit((list->display[i]).screenName);
				dpi.input = (list->display[i]).index;
				properties.valid.append(dpi);

				_deviceProperties.insert(id, properties);
			}


			_releaseX11Displays(list);
		}
	}	
}

bool X11Grabber::start()
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

void X11Grabber::stop()
{
	if (_initialized)
	{
		_semaphore.acquire();
		_timer.stop();
		if (_handle != nullptr)
		{
			_uninitX11Display(_handle);
			_handle = nullptr;
			_initialized = false;
		}
		_semaphore.release();
		Info(_log, "Stopped");
	}
}

bool X11Grabber::init_device(int _display)
{
	_actualDisplay = _display;
	_handle = _initX11Display(_display);

	if (_handle == nullptr)	
		Error(_log, "Could not initialized x11 grabber");

	return (_handle != nullptr);
}

void X11Grabber::grabFrame()
{
	bool stopNow = false;
	
	if (_semaphore.tryAcquire())
	{
		if (_initialized && _handle != nullptr)
		{
			unsigned char* data = _getFrame(_handle);

			if (data == nullptr)
			{
				Error(_log, "Could not capture x11 frame");
				stopNow = true;
			}
			else
			{
				_actualWidth = _handle->width;
				_actualHeight = _handle->height;

				processSystemFrameBGRA(data);

				_releaseFrame(_handle);
			}
		}
		_semaphore.release();
	}

	if (stopNow)
	{
		uninit();
	}
}


void X11Grabber::setCropping(unsigned cropLeft, unsigned cropRight, unsigned cropTop, unsigned cropBottom)
{
	_cropLeft = cropLeft;
	_cropRight = cropRight;
	_cropTop = cropTop;
	_cropBottom = cropBottom;
}
