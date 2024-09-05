/* macOsGrabber.cpp
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

#include <grabber/osx/macOS/macOsGrabber.h>

#import <Foundation/Foundation.h>
#import <Foundation/NSProcessInfo.h>

id activity;

macOsGrabber::macOsGrabber(const QString& device, const QString& configurationPath)
	: Grabber(configurationPath, "MACOS_SYSTEM:" + device.left(14))
	, _configurationPath(configurationPath)
	, _actualDisplay(0)
{
	_timer.setTimerType(Qt::PreciseTimer);
	connect(&_timer, &QTimer::timeout, this, &macOsGrabber::grabFrame);

	// Refresh devices
	getDevices();

	//
	activity = [[NSProcessInfo processInfo]beginActivityWithOptions:NSActivityBackground reason : @"HyperHDR background activity"];
	[activity retain] ;
}

QString macOsGrabber::GetSharedLut()
{
	return "";
}

void macOsGrabber::loadLutFile(PixelFormat color)
{		
}

void macOsGrabber::setHdrToneMappingEnabled(int mode)
{
}

macOsGrabber::~macOsGrabber()
{
	uninit();

	// destroy core elements from contructor

	[[NSProcessInfo processInfo]endActivity:activity];
	[activity release];
}

void macOsGrabber::uninit()
{
	// stop if the grabber was not stopped
	if (_initialized)
	{		
		stop();
		Debug(_log, "Uninit grabber: %s", QSTRING_CSTR(_deviceName));
	}

	_actualDisplay = 0;

	_initialized = false;
}

bool macOsGrabber::init()
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
			Error(_log, "Could not find any capture device");
			return false;
		}

		
		Info(_log, "*************************************************************************************************");
		Info(_log, "Starting macOS grabber. Selected: '%s' max width: %d (%d) @ %d fps", QSTRING_CSTR(foundDevice), _width, _height, _fps);
		Info(_log, "*************************************************************************************************");

		_actualDisplay = _deviceProperties[foundDevice].valid.first().display;
		_initialized = true;		
	}

	return _initialized;
}


void macOsGrabber::getDevices()
{
	enumerateDevices(false);
}

void macOsGrabber::enumerateDevices(bool silent)
{
	_deviceProperties.clear();

	CGDisplayCount    screenCount;
	CGDisplayCount    maxDisplays = 16;
	CGDirectDisplayID onlineDisplays[16];

	CGDisplayErr status = CGGetOnlineDisplayList(maxDisplays, onlineDisplays, &screenCount);

	if (status != kCGErrorSuccess)
	{
		Error(_log, "Could not find any display (error: %d)", status);
		return;
	}

	for (CGDisplayCount i = 0; i < screenCount; i++)
	{
		CGDirectDisplayID dID = onlineDisplays[i];
		QString id = QString("Display id: %1").arg(dID);

		if (!silent)
			Info(_log, "Found display: %s", QSTRING_CSTR(id));

		DeviceProperties properties;
		DevicePropertiesItem dpi;

		dpi.display = dID;
		properties.valid.append(dpi);

		_deviceProperties.insert(id, properties);
	}	
}

bool macOsGrabber::start()
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

void macOsGrabber::stop()
{
	if (_initialized)
	{
		_timer.stop();
		Info(_log, "Stopped");
	}
}

bool macOsGrabber::init_device(QString selectedDeviceName)
{	
	return true;
}

void macOsGrabber::grabFrame()
{
	bool stopNow = false;
		
		CGImageRef display;
		CFDataRef sysData;
		uint8_t* rawData;

		display = CGDisplayCreateImage(_actualDisplay);

		if (display == NULL)
		{			
			Error(_log, "Lost connection to the display or user didn't grant access rights");
			stopNow = true;
		}
		else
		{			
			sysData = CGDataProviderCopyData(CGImageGetDataProvider(display));;

			if (sysData != NULL)
			{
				rawData = (uint8_t*)CFDataGetBytePtr(sysData);
				_actualWidth = CGImageGetWidth(display);
				_actualHeight = CGImageGetHeight(display);

				size_t bytesPerRow = CGImageGetBytesPerRow(display);
				processSystemFrameBGRA(rawData, static_cast<int>(bytesPerRow));

				CFRelease(sysData);
			}

			CGImageRelease(display);
		}

	if (stopNow)
	{
		uninit();
		QTimer::singleShot(3000, this, &macOsGrabber::start);
	}
}


void macOsGrabber::setCropping(unsigned cropLeft, unsigned cropRight, unsigned cropTop, unsigned cropBottom)
{
	_cropLeft = cropLeft;
	_cropRight = cropRight;
	_cropTop = cropTop;
	_cropBottom = cropBottom;
}
