/* PipewireGrabber.cpp
*
*  MIT License
*
*  Copyright (c) 2023 awawa-dev
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
#include <base/HyperHdrIManager.h>
#include <base/AuthManager.h>

#include <QDirIterator>
#include <QFileInfo>
#include <QCoreApplication>

#include <grabber/PipewireGrabber.h>
#include <base/SystemWrapper.h>
#include <grabber/PipewireWrapper.h>
#include <utils/ColorSys.h>
#include <dlfcn.h>

bool (*_hasPipewire)() = nullptr;
const char* (*_getPipewireError)() = nullptr;
void (*_initPipewireDisplay)(const char* restorationToken, uint32_t requestedFPS, pipewire_callback_func callback) = nullptr;
void (*_uninitPipewireDisplay)() = nullptr;
const char* (*_getPipewireToken)() = nullptr;

PipewireGrabber::PipewireGrabber(const QString& device, const QString& configurationPath)
	: Grabber("PIPEWIRE_SYSTEM:" + device.left(14))
	, _configurationPath(configurationPath)
	, _library(nullptr)
	, _actualDisplay(0)
	, _isActive(false)
	, _storedToken(false)
	, _versionCheck(false)
	, _hasFrame(false)
{
	// Load library
	_library = dlopen("libsmartPipewire.so", RTLD_NOW);

	if (_library)
	{
		_getPipewireToken = (const char* (*)()) dlsym(_library, "getPipewireToken");
		_getPipewireError = (const char* (*)()) dlsym(_library, "getPipewireError");
		_hasPipewire = (bool (*)()) dlsym(_library, "hasPipewire");
		_initPipewireDisplay = (void (*)(const char*, uint32_t, pipewire_callback_func)) dlsym(_library, "initPipewireDisplay");
		_uninitPipewireDisplay = (void (*)()) dlsym(_library, "uniniPipewireDisplay");
	}
	else
		Warning(_log, "Could not load Pipewire proxy library. Error: %s", dlerror());

	if (_library && (_getPipewireToken == nullptr || _hasPipewire == nullptr || _initPipewireDisplay == nullptr || _uninitPipewireDisplay == nullptr ))
	{
		Error(_log, "Could not load Pipewire proxy library definition. Error: %s", dlerror());

		dlclose(_library);
		_library = nullptr;
	}	
	
	// Refresh devices
	if (_library)
	{
		Info(_log, "Loaded Pipewire proxy library for screen capturing");
		getDevices();
	}	
}

bool PipewireGrabber::hasPipewire(bool force)
{
	if (_library == nullptr)
		return false;

	bool retVal = _hasPipewire();
	if (!retVal && force)
	{
		Warning(_log, "Portal grabber is not recommended but it's forced by the user");
		retVal = true;
	}

	return retVal;
}

QString PipewireGrabber::GetSharedLut()
{
	return "";
}

void PipewireGrabber::loadLutFile(PixelFormat color)
{		
}

void PipewireGrabber::setHdrToneMappingEnabled(int mode)
{
}

PipewireGrabber::~PipewireGrabber()
{
	uninit();

	if (_library)
	{
		_uninitPipewireDisplay();
		dlclose(_library);
		_library = nullptr;
	}
}

void PipewireGrabber::uninit()
{
	// stop if the grabber was not stopped
	if (_initialized)
	{		
		stop();
		Debug(_log, "Uninit grabber: %s", QSTRING_CSTR(_deviceName));
	}
	
	_initialized = false;
}

bool PipewireGrabber::init()
{
	Debug(_log, "init");

	_hasFrame = false;

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
			Error(_log, "Could not find any capture device. Pipewire system grabber won't work if HyperHDR is run as a service due to security restrictions.");
			return false;
		}

		
		Info(_log, "*************************************************************************************************");
		Info(_log, "Starting Pipewire grabber. Selected: '%s' (%i) max width: %d (%d) @ %d fps", QSTRING_CSTR(foundDevice), _deviceProperties[foundDevice].valid.first().input, _width, _height, _fps);
		Info(_log, "*************************************************************************************************");		

		if (init_device(_deviceProperties[foundDevice].valid.first().input))
			_initialized = true;		
	}

	return _initialized;
}


void PipewireGrabber::getDevices()
{
	enumerateDevices(false);
}

void PipewireGrabber::enumerateDevices(bool silent)
{
	_deviceProperties.clear();

	if (_library != nullptr)
	{
		DeviceProperties properties;
		DevicePropertiesItem dpi;

		QString id = "Pipewire System Dialog selection";
		dpi.input = 1;
		properties.valid.append(dpi);

		_deviceProperties.insert(id, properties);		
	}	
}

bool PipewireGrabber::start()
{
	try
	{		
		if (init())
		{
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

void PipewireGrabber::stop()
{
	if (_initialized)
	{
		_uninitPipewireDisplay();
		_isActive = false;
		_initialized = false;
		_hasFrame = false;
		
		Info(_log, "Stopped");
	}
}

bool PipewireGrabber::init_device(int _display)
{
	_actualDisplay = _display;
	_storedToken = false;
	_versionCheck = false;


	AuthManager* instance = AuthManager::getInstance();
	QString token = instance->loadPipewire();
	if (token.isNull())
		token = "";
	else
		Info(_log, "Loading restoration token: %s", QSTRING_CSTR(maskToken(token)));
	_initPipewireDisplay(token.toLatin1().constData(), _fps, (pipewire_callback_func) &PipewireGrabber::callbackFunction);

	_isActive = true;

	if (!_isActive)
		Error(_log, "Could not initialized Pipewire grabber");

	return _isActive;
}

QString PipewireGrabber::maskToken(const QString& token) const
{
	QString retVal = token;

	for (int i = 0; i < 24 && i < retVal.length(); i++)
	{
		retVal[i] = '*';
	}

	return retVal;
}

void PipewireGrabber::stateChanged(bool state)
{
	if (!state)
	{
		AuthManager* instance = AuthManager::getInstance();

		Info(_log, "Removing restoration token");

		instance->savePipewire("");
	}
}

void PipewireGrabber::grabFrame(const PipewireImage& data)
{
	bool stopNow = false;
	
	if (_initialized && _isActive)
	{
		if (!_versionCheck)
		{
			if (data.version >= 4)
				Info(_log, "Portal protocol version: %i", data.version);
			else
				Warning(_log, "Legacy portal protocol version: %i. To enjoy persistant autorization since version 4, you should update xdg-desktop-portal at least to version 1.12.1 *AND* provide backend that can implement it (for example newest xdg-desktop-portal-gnome).", data.version);

			_versionCheck = true;
		}


		if (!_storedToken && !data.isError)
		{
			QString token = QString("%1").arg(_getPipewireToken());

			if (!token.isEmpty())
			{
				AuthManager* instance = AuthManager::getInstance();

				Info(_log, "Saving restoration token: %s", QSTRING_CSTR(maskToken(token)));

				instance->savePipewire(token);

				_storedToken = true;
			}
		}			


		if (data.data == nullptr)
		{
			if (data.isError)
			{
				QString err = QString("%1").arg(_getPipewireError());
				Error(_log, "Could not capture pipewire frame: %s", QSTRING_CSTR(err));
				stopNow = true;
			}
		}
		else
		{
			_hasFrame = true;
			_actualWidth = data.width;
			_actualHeight = data.height;

			if (data.isOrderRgb)
				processSystemFrameRGBA(data.data, data.stride);
			else
				processSystemFrameBGRA(data.data, data.stride);
		}
	}

	if (stopNow)
	{
		uninit();
	}
}


void PipewireGrabber::setCropping(unsigned cropLeft, unsigned cropRight, unsigned cropTop, unsigned cropBottom)
{
	_cropLeft = cropLeft;
	_cropRight = cropRight;
	_cropTop = cropTop;
	_cropBottom = cropBottom;
}

void PipewireGrabber::callbackFunction(const PipewireImage& frame)
{
	if (SystemWrapper::getInstance() == nullptr)
		return;

	PipewireWrapper* wrapper = dynamic_cast<PipewireWrapper*>(SystemWrapper::getInstance());

	if (wrapper != NULL)
	{
		if (QThread::currentThread() == wrapper->thread())
			wrapper->processFrame(frame);
		else
			QMetaObject::invokeMethod(wrapper, "processFrame", Qt::ConnectionType::BlockingQueuedConnection, Q_ARG(const PipewireImage&, frame));
	}
}

bool PipewireGrabber::isRunning()
{
	return _initialized && _hasFrame;
}
