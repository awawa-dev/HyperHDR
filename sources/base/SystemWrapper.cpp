/* SystemWrapper.cpp
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

#ifndef PCH_ENABLED
	#include <QTimer>
	#include <QThread>
#endif

#include <HyperhdrConfig.h>
#include <base/SystemWrapper.h>
#include <base/Grabber.h>
#include <utils/GlobalSignals.h>
#include <base/HyperHdrManager.h>

SystemWrapper::SystemWrapper(const QString& grabberName, Grabber* ggrabber)
	: _grabberName(grabberName)
	, _log(Logger::getInstance(grabberName))
	, _configLoaded(false)
	, _grabber(ggrabber)
{
	// connect the image forwarding

	connect(this, &SystemWrapper::SignalSystemImage, GlobalSignals::getInstance(), &GlobalSignals::SignalNewSystemImage);

	// listen for source requests
	connect(GlobalSignals::getInstance(), &GlobalSignals::SignalRequestComponent, this, &SystemWrapper::signalRequestSourceHandler);
}

void SystemWrapper::newCapturedFrameHandler(const Image<ColorRgb>& image)
{
	emit SignalSystemImage(_grabberName, image);
}

void SystemWrapper::stateChanged(bool state)
{

}

void SystemWrapper::capturingExceptionHandler(const char* err)
{
	Error(_log, "Grabber signals error (%s)", err);
}

SystemWrapper::~SystemWrapper()
{
	Debug(_log, "Closing grabber: %s", QSTRING_CSTR(_grabberName));
}

void SystemWrapper::signalRequestSourceHandler(hyperhdr::Components component, int hyperhdrInd, bool listen)
{
	if (component == hyperhdr::Components::COMP_SYSTEMGRABBER)
	{
		if (listen && !GRABBER_SYSTEM_CLIENTS.contains(hyperhdrInd))
			GRABBER_SYSTEM_CLIENTS.append(hyperhdrInd);
		else if (!listen)
			GRABBER_SYSTEM_CLIENTS.removeOne(hyperhdrInd);

		if (GRABBER_SYSTEM_CLIENTS.empty())
			stop();
		else
			start();
	}
}

bool SystemWrapper::start()
{
	if (_grabber != nullptr)
		return _grabber->start();
	else
		return false;
}

void SystemWrapper::stop()
{
	if (_grabber != nullptr)
		_grabber->stop();
}

void SystemWrapper::setSignalThreshold(double redSignalThreshold, double greenSignalThreshold, double blueSignalThreshold, int noSignalCounterThreshold)
{
	if (_grabber != nullptr)
		_grabber->setSignalThreshold(redSignalThreshold, greenSignalThreshold, blueSignalThreshold, noSignalCounterThreshold);
}

void SystemWrapper::setSignalDetectionOffset(double verticalMin, double horizontalMin, double verticalMax, double horizontalMax)
{
	if (_grabber != nullptr)
		_grabber->setSignalDetectionOffset(verticalMin, horizontalMin, verticalMax, horizontalMax);
}

void SystemWrapper::setSignalDetectionEnable(bool enable)
{
	if (_grabber != nullptr)
		_grabber->setSignalDetectionEnable(enable);
}

void SystemWrapper::setDeviceVideoStandard(const QString& device)
{
	if (_grabber != nullptr)
		_grabber->setDeviceVideoStandard(device);
}

void SystemWrapper::setHdrToneMappingEnabled(int mode)
{
	if (_grabber != NULL)
	{
		_grabber->setHdrToneMappingEnabled(mode);
	}
}

void SystemWrapper::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if (type == settings::type::SYSTEMGRABBER && _grabber != nullptr)
	{
		try
		{
			_grabber->setBlocked();

			// extract settings
			const QJsonObject& obj = config.object();

			// crop for video
			_grabber->setCropping(
				obj["cropLeft"].toInt(0),
				obj["cropRight"].toInt(0),
				obj["cropTop"].toInt(0),
				obj["cropBottom"].toInt(0));

			// device resolution
			int videoMode = obj["videoMode"].toInt(512);
			_grabber->setWidthHeight(videoMode, videoMode);

			// device framerate
			_grabber->setFramerate(obj["fps"].toInt(10));

#ifdef ENABLE_DX
			// HDR tone mapping
			setHdrToneMappingEnabled(obj["hdrToneMapping"].toBool(false) ? 1 : 0);
#endif

			// signal
			_grabber->setSignalDetectionEnable(obj["signalDetection"].toBool(false));

			_grabber->enableHardwareAcceleration(obj["hardware"].toBool(false));

			_grabber->setMonitorNits(obj["monitor_nits"].toInt(200));

			_grabber->setSignalDetectionOffset(
				obj["sDHOffsetMin"].toDouble(0.25),
				obj["sDVOffsetMin"].toDouble(0.25),
				obj["sDHOffsetMax"].toDouble(0.75),
				obj["sDVOffsetMax"].toDouble(0.75));

			_grabber->setSignalThreshold(
				obj["redSignalThreshold"].toDouble(0.0) / 100.0,
				obj["greenSignalThreshold"].toDouble(0.0) / 100.0,
				obj["blueSignalThreshold"].toDouble(0.0) / 100.0,
				obj["noSignalCounterThreshold"].toInt(50));

			_grabber->setDeviceVideoStandard(obj["device"].toString(Grabber::AUTO_SETTING));

			_grabber->unblockAndRestart(_configLoaded);
		}
		catch (...)
		{
			_grabber->unblockAndRestart(_configLoaded);
		}
		_configLoaded = true;
	}
}

QString SystemWrapper::getGrabberInfo()
{
#if defined(ENABLE_DX)
	return "DirectX11";
#endif
#if defined(ENABLE_MAC_SYSTEM)
	return "macOS";
#endif
#if defined(ENABLE_X11)
	return "X11";
#endif
	return "";
}

bool SystemWrapper::isActivated(bool forced)
{
	return true;
}

QJsonObject SystemWrapper::getJsonInfo()
{
	QJsonObject systemDevice;
	QJsonArray  systemModes;

	systemDevice["device"] = getGrabberInfo();

	QStringList list = _grabber->getVideoDevices();

	for (const auto& devicePath : list)
	{
		systemModes.append(devicePath);
	}

	systemDevice["modes"] = systemModes;

	return systemDevice;
}
