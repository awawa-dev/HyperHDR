/* GrabberWrapper.cpp
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

#include <base/GrabberWrapper.h>
#include <base/Grabber.h>
#include <utils/VideoMemoryManager.h>
#include <HyperhdrConfig.h>

// utils includes
#include <utils/GlobalSignals.h>
#include <utils/QStringUtils.h>
#include <base/HyperHdrIManager.h>

// qt
#include <QTimer>
#include <QThread>

GrabberWrapper* GrabberWrapper::instance = nullptr;

GrabberWrapper::GrabberWrapper(const QString& grabberName, Grabber* ggrabber)
	: _grabberName(grabberName)
	, _log(Logger::getInstance(grabberName))
	, _configLoaded(false)
	, _grabber(ggrabber)
	, _cecHdrStart(0)
	, _cecHdrStop(0)
	, _autoResume(false)
	, _isPaused(false)
	, _pausingModeEnabled(false)
	, _benchmarkStatus(-1)
	, _benchmarkMessage("")
{
	GrabberWrapper::instance = this;

	// connect the image forwarding

	connect(this, &GrabberWrapper::systemImage, GlobalSignals::getInstance(), &GlobalSignals::setVideoImage);

	// listen for source requests
	connect(GlobalSignals::getInstance(), &GlobalSignals::requestSource, this, &GrabberWrapper::handleSourceRequest);

	connect(this, &GrabberWrapper::cecKeyPressed, this, &GrabberWrapper::cecKeyPressedHandler);

	connect(this, &GrabberWrapper::setBrightnessContrastSaturationHue, this, &GrabberWrapper::setBrightnessContrastSaturationHueHandler);

	connect(this, &GrabberWrapper::instancePauseChanged, this, &GrabberWrapper::instancePauseChangedHandler);
}

void GrabberWrapper::instancePauseChangedHandler(int instance, bool isEnabled)
{
	static int signature = 0;
	int trigger = 0;

	if (!_pausingModeEnabled)
		return;

	if (!isEnabled)
	{
		if (!_paused_clients.contains(instance))
		{
			_paused_clients.append(instance);
			trigger = 10000;
		}
	}
	else if (isEnabled)
	{
		if (_paused_clients.contains(instance))
		{
			_paused_clients.removeOne(instance);
			trigger = 1000;
		}
	}

	if (trigger > 0)
	{
		int newSignature = ++signature;
		QTimer::singleShot(trigger, Qt::TimerType::PreciseTimer, this, [this, newSignature]() {
			if (signature == newSignature)
			{
				if (_paused_clients.length() == _running_clients.length() && _paused_clients.length() > 0)
				{
					if (!_isPaused)
					{
						Warning(_log, "LEDs are off and you have enabled the pausing feature for the USB grabber. Pausing the video grabber now.");
						auto _running_clients_copy = _running_clients;
						_running_clients.clear();
						handleSourceRequest(hyperhdr::Components::COMP_VIDEOGRABBER, -1, false);
						_running_clients = _running_clients_copy;
						_isPaused = true;						
					}
				}
				else if (_paused_clients.length() < _running_clients.length() && _running_clients.length() > 0)
				{
					if (_isPaused)
					{
						Warning(_log, "LEDs are on. Resuming the video grabber now.");
						handleSourceRequest(hyperhdr::Components::COMP_VIDEOGRABBER, -1, true);
						_isPaused = false;
					}
				}
			}
		});
	}
}

void GrabberWrapper::setCecStartStop(int cecHdrStart, int cecHdrStop)
{
	_cecHdrStart = cecHdrStart;
	_cecHdrStop = cecHdrStop;

	Debug(_log, "CEC keycode. Start: %i, stop: %i", _cecHdrStart, _cecHdrStop);
}

QJsonDocument GrabberWrapper::startCalibration()
{
	if (_grabber == nullptr)
		return QJsonDocument();

	if (QThread::currentThread() == _grabber->thread())	
		return _grabber->startCalibration();

	QJsonDocument retVal;
	QMetaObject::invokeMethod(_grabber, "startCalibration", Qt::ConnectionType::BlockingQueuedConnection,
		Q_RETURN_ARG(QJsonDocument, retVal));

	return retVal;
}

QJsonDocument GrabberWrapper::stopCalibration()
{
	if (_grabber == nullptr)
		return QJsonDocument();

	if (QThread::currentThread() == _grabber->thread())
		return _grabber->stopCalibration();

	QJsonDocument retVal;
	QMetaObject::invokeMethod(_grabber, "stopCalibration", Qt::ConnectionType::BlockingQueuedConnection,
		Q_RETURN_ARG(QJsonDocument, retVal));

	return retVal;
}

QJsonDocument GrabberWrapper::getCalibrationInfo()
{
	if (_grabber == nullptr)
		return QJsonDocument();

	if (QThread::currentThread() == _grabber->thread())
		return _grabber->getCalibrationInfo();	

	QJsonDocument retVal;
	QMetaObject::invokeMethod(_grabber, "getCalibrationInfo", Qt::ConnectionType::BlockingQueuedConnection,
		Q_RETURN_ARG(QJsonDocument, retVal));

	return retVal;
}

void GrabberWrapper::cecKeyPressedHandler(int key)
{
	if (_grabber != nullptr)
	{
		if (key == _cecHdrStart && getHdrToneMappingEnabled() != 1)
		{
			setHdrToneMappingEnabled(1);
		}
		else if (key == _cecHdrStop && getHdrToneMappingEnabled() != 0)
		{
			setHdrToneMappingEnabled(0);
		}
	}
}

bool GrabberWrapper::isCEC()
{
	return (_cecHdrStart > 0) || (_cecHdrStop > 0);
}

void GrabberWrapper::newFrame(const Image<ColorRgb>& image)
{
	if (_benchmarkStatus >= 0)
	{
		ColorRgb pixel = image(image.width() / 2, image.height() / 2);
		if ((_benchmarkMessage == "white" && pixel.red > 120 && pixel.green > 120 && pixel.blue > 120) ||
			(_benchmarkMessage == "red" && pixel.red > 120 && pixel.green < 30 && pixel.blue < 30) ||
			(_benchmarkMessage == "green" && pixel.red < 30 && pixel.green > 120 && pixel.blue < 30) ||
			(_benchmarkMessage == "blue" && pixel.red < 30 && pixel.green < 40 && pixel.blue > 120) ||
			(_benchmarkMessage == "black" && pixel.red < 30 && pixel.green < 30 && pixel.blue < 30))

		{
			emit benchmarkUpdate(_benchmarkStatus, _benchmarkMessage);
			_benchmarkStatus = -1;
			_benchmarkMessage = "";
		}
	}

	if (image.setBufferCacheSize())
	{
		Warning(_log, "Detected the video frame size changed (%ix%i). Cache buffer was cleared.", image.width(), image.height());
	}

	emit systemImage(_grabberName, image);
}

void GrabberWrapper::readError(const char* err)
{
	Error(_log, "Grabber signals error (%s)", err);
}

GrabberWrapper::~GrabberWrapper()
{
	Debug(_log, "Closing grabber: %s", QSTRING_CSTR(_grabberName));
}

QJsonObject GrabberWrapper::getJsonInfo()
{
	if (_grabber == nullptr)
		return QJsonObject();

	QJsonArray availableGrabbers;
	QJsonObject grabbers;

	if (QThread::currentThread() == _grabber->thread())
		grabbers = _grabber->getJsonInfo();
	else
		QMetaObject::invokeMethod(_grabber, "getJsonInfo", Qt::ConnectionType::BlockingQueuedConnection, Q_RETURN_ARG(QJsonObject, grabbers));

	grabbers["active"] = _grabberName;

#ifdef ENABLE_V4L2
	availableGrabbers.append("Video capturing (V4L2)");
#endif

#ifdef ENABLE_MF
	availableGrabbers.append("Video capturing (MF)");
#endif

#ifdef ENABLE_AVF
	availableGrabbers.append("Video capturing (AVF)");
#endif

	grabbers["available"] = availableGrabbers;

	return grabbers;
}

void GrabberWrapper::handleSourceRequest(hyperhdr::Components component, int instanceIndex, bool listen)
{
	if (component == hyperhdr::Components::COMP_VIDEOGRABBER)
	{
		if (instanceIndex >= 0)
		{
			if (listen && !_running_clients.contains(instanceIndex))
			{
				_running_clients.append(instanceIndex);
				_paused_clients.removeOne(instanceIndex);
			}
			else if (!listen)
			{
				_running_clients.removeOne(instanceIndex);
				_paused_clients.removeOne(instanceIndex);
			}
		}

		if (_running_clients.empty())
		{
			stop();

			//update state
			QString currentDevice = "", currentVideoMode = "";
			emit StateChanged(currentDevice, currentVideoMode);

			emit PerformanceCounters::getInstance()->removeCounter(static_cast<int>(PerformanceReportType::VIDEO_GRABBER), -1);
		}
		else
		{
			bool result = start();

			// update state
			QString currentDevice = "", currentVideoMode = "";
			auto currentInfo = getVideoCurrentMode();

			if (currentInfo.contains(Grabber::currentVideoModeInfo::device))
				currentDevice = currentInfo[Grabber::currentVideoModeInfo::device];

			if (currentInfo.contains(Grabber::currentVideoModeInfo::resolution))
				currentVideoMode = currentInfo[Grabber::currentVideoModeInfo::resolution];

			emit StateChanged(currentDevice, currentVideoMode);

			if (result)
				emit PerformanceCounters::getInstance()->newCounter(
					PerformanceReport(static_cast<int>(PerformanceReportType::VIDEO_GRABBER), -1, "", -1, -1, -1, -1));
			else
				emit PerformanceCounters::getInstance()->removeCounter(static_cast<int>(PerformanceReportType::VIDEO_GRABBER), -1);
		}
	}
}


QMap<Grabber::currentVideoModeInfo, QString> GrabberWrapper::getVideoCurrentMode() const
{	
	if (_grabber != nullptr)
		return _grabber->getVideoCurrentMode();
	else
		return QMap<Grabber::currentVideoModeInfo, QString>();
}

bool GrabberWrapper::start()
{
	if (_grabber != nullptr)
		return _grabber->start();
	else
		return false;
}

void GrabberWrapper::stop()
{
	emit PerformanceCounters::getInstance()->removeCounter(static_cast<int>(PerformanceReportType::VIDEO_GRABBER), -1);

	if (_grabber != nullptr)
		_grabber->stop();
}

void GrabberWrapper::setSignalThreshold(double redSignalThreshold, double greenSignalThreshold, double blueSignalThreshold, int noSignalCounterThreshold)
{
	if (_grabber != nullptr)
		_grabber->setSignalThreshold(redSignalThreshold, greenSignalThreshold, blueSignalThreshold, noSignalCounterThreshold);
}

void GrabberWrapper::setCropping(unsigned cropLeft, unsigned cropRight, unsigned cropTop, unsigned cropBottom)
{
	if (_grabber != nullptr)
		_grabber->setCropping(cropLeft, cropRight, cropTop, cropBottom);
}

void GrabberWrapper::setSignalDetectionOffset(double verticalMin, double horizontalMin, double verticalMax, double horizontalMax)
{
	if (_grabber != nullptr)
		_grabber->setSignalDetectionOffset(verticalMin, horizontalMin, verticalMax, horizontalMax);
}

void GrabberWrapper::setSignalDetectionEnable(bool enable)
{
	if (_grabber != nullptr)
		_grabber->setSignalDetectionEnable(enable);
}

void GrabberWrapper::setDeviceVideoStandard(const QString& device)
{
	if (_grabber != nullptr)
		_grabber->setDeviceVideoStandard(device);
}

void GrabberWrapper::setHdrToneMappingEnabled(int mode)
{
	if (_grabber != NULL)
	{
		_grabber->setHdrToneMappingEnabled(mode);

		// make emit
		emit HdrChanged(mode);
		emit HyperHdrIManager::getInstance()->setNewComponentStateToAllInstances(hyperhdr::Components::COMP_HDR, (mode != 0));
	}
}

int GrabberWrapper::getHdrToneMappingEnabled()
{
	if (_grabber != nullptr)
		return _grabber->getHdrToneMappingEnabled();
	else
		return 0;
}

int GrabberWrapper::getFpsSoftwareDecimation()
{
	return _grabber->getFpsSoftwareDecimation();
}

int GrabberWrapper::getActualFps()
{
	return _grabber->getActualFps();
}

void GrabberWrapper::setFpsSoftwareDecimation(int decimation)
{
	if (_grabber != nullptr)
		_grabber->setFpsSoftwareDecimation(decimation);
}

void GrabberWrapper::setEncoding(QString enc)
{
	if (_grabber != nullptr)
		_grabber->setEncoding(enc);
}

void GrabberWrapper::setBrightnessContrastSaturationHueHandler(int brightness, int contrast, int saturation, int hue)
{
	if (_grabber != nullptr)
		_grabber->setBrightnessContrastSaturationHue(brightness, contrast, saturation, hue);
}

void GrabberWrapper::setQFrameDecimation(int setQframe)
{
	if (_grabber != nullptr)
		_grabber->setQFrameDecimation(setQframe);
}

DetectionAutomatic::calibrationPoint GrabberWrapper::parsePoint(int width, int height, QJsonObject element, bool& ok)
{
	DetectionAutomatic::calibrationPoint p;

	p.x = element["x"].toInt(-1);
	p.y = element["y"].toInt(-1);

	if (p.x < 0 || p.y < 0 || p.x >= width || p.y >= height ||
		!element["color"].isArray() || element["color"].toArray().size() != 3)
	{
		ok = false;
		return p;
	}

	QJsonArray color = element["color"].toArray();
	p.r = color[0].toInt();
	p.g = color[1].toInt();
	p.b = color[2].toInt();

	if (p.r > 255 || p.g > 255 || p.b > 255 || p.r < 0 || p.g < 0 || p.b < 0)
		ok = false;
	else
		ok = true;

	return p;
}

void GrabberWrapper::revive()
{
	if (_grabber != nullptr && _autoResume)
		QTimer::singleShot(3000, _grabber, SLOT(revive()));
}

void GrabberWrapper::benchmarkCapture(int status, QString message)
{
	_benchmarkStatus = status;
	_benchmarkMessage = message;
}

bool GrabberWrapper::getAutoResume()
{
	return _autoResume;
}

void GrabberWrapper::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if (type == settings::type::VIDEODETECTION && _grabber != nullptr)
	{
		// extract settings
		const QJsonObject& obj = config.object();

		QString signature = obj["signature"].toString("");
		int quality = obj["quality"].toInt(0);
		int width = obj["width"].toInt(0);
		int height = obj["height"].toInt(0);
		QJsonArray sdr = obj["calibration_sdr"].toArray(QJsonArray());
		QJsonArray hdr = obj["calibration_hdr"].toArray(QJsonArray());
		std::vector<DetectionAutomatic::calibrationPoint> sdrVec, hdrVec;

		for (auto v : sdr) {
			bool ok = false;
			QJsonObject element = v.toObject();
			DetectionAutomatic::calibrationPoint p;

			p = parsePoint(width, height, element, ok);

			if (ok)
				sdrVec.push_back(p);
			else
				Warning(_log, "Calibration data are damaged");
		}

		for (auto v : hdr) {
			bool ok = false;
			QJsonObject element = v.toObject();
			DetectionAutomatic::calibrationPoint p;

			p = parsePoint(width, height, element, ok);

			if (ok)
				hdrVec.push_back(p);
			else
				Warning(_log, "Calibration data are damaged");
		}

		_grabber->setAutomaticCalibrationData(signature, quality, width, height, sdrVec, hdrVec);
	}

	if (type == settings::type::VIDEOGRABBER && _grabber != nullptr)
	{
		try
		{
			_grabber->setBlocked();

			// extract settings
			const QJsonObject& obj = config.object();

			// auto resume
			if (_autoResume != obj["autoResume"].toBool(false))
			{
				_autoResume = obj["autoResume"].toBool(false);
				Debug(_log, "Auto resume is: %s", (_autoResume) ? "enabled" : "disabled");
			}

			if (_pausingModeEnabled != obj["led_off_pause"].toBool(false))
			{
				_pausingModeEnabled = obj["led_off_pause"].toBool(false);
				Debug(_log, "Pausing mode is: %s", (_pausingModeEnabled) ? "enabled" : "disabled");
			}

			// crop for video
			_grabber->setCropping(
				obj["cropLeft"].toInt(0),
				obj["cropRight"].toInt(0),
				obj["cropTop"].toInt(0),
				obj["cropBottom"].toInt(0));

			// device input
			_grabber->setInput(obj["input"].toInt(-1));

			setCecStartStop(obj["cecHdrStart"].toInt(0), obj["cecHdrStop"].toInt(0));

			// device resolution
			QString videoMode = obj["videoMode"].toString("0x0").toLower();
			QStringList res = QStringUtils::SPLITTER(videoMode, 'x');
			bool okX = false, okY = false;
			int x = 0, y = 0;

			if (res.length() == 2)
			{
				x = res[0].toInt(&okX);
				y = res[1].toInt(&okY);
			}

			if (!okX || !okY)
				_grabber->setWidthHeight(0, 0);
			else
				_grabber->setWidthHeight(x, y);

			// device framerate
			_grabber->setFramerate(obj["fps"].toInt(Grabber::AUTO_FPS));

			_grabber->setBrightnessContrastSaturationHue(
				obj["hardware_brightness"].toInt(0),
				obj["hardware_contrast"].toInt(0),
				obj["hardware_saturation"].toInt(0),
				obj["hardware_hue"].toInt(0));


			// HDR tone mapping
			setHdrToneMappingEnabled(obj["hdrToneMapping"].toBool(false) ? obj["hdrToneMappingMode"].toInt(1) : 0);

			// software frame skipping
			_grabber->setFpsSoftwareDecimation(obj["fpsSoftwareDecimation"].toInt(1));

			// old dump signal detection
			_grabber->setSignalDetectionEnable(obj["signalDetection"].toBool(false));

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

			// new smart signal detection
			_grabber->setAutoSignalDetectionEnable(obj["autoSignalDetection"].toBool(false));

			_grabber->setAutoSignalParams(
				obj["saveResources"].toBool(false),
				obj["errorTolerance"].toInt(9),
				obj["modelTolerance"].toInt(90),
				obj["sleepTime"].toInt(1000),
				obj["wakeTime"].toInt(0));

			_grabber->setDeviceVideoStandard(obj["device"].toString(Grabber::AUTO_SETTING));

			_grabber->setEncoding(obj["videoEncoding"].toString(pixelFormatToString(PixelFormat::NO_CHANGE)));

			_grabber->setQFrameDecimation(obj["qFrame"].toBool(false));

			bool frameCache = obj["videoCache"].toBool(true);
			Debug(_log, "Frame cache is: %s", (frameCache) ? "enabled" : "disabled");
			VideoMemoryManager::enableCache(frameCache);

			_grabber->unblockAndRestart(_configLoaded);
		}
		catch (...)
		{
			_grabber->unblockAndRestart(_configLoaded);
		}
		_configLoaded = true;

		// update info
		QString currentDevice = "", currentVideoMode = "";
		auto currentInfo = getVideoCurrentMode();

		if (currentInfo.contains(Grabber::currentVideoModeInfo::device))
			currentDevice = currentInfo[Grabber::currentVideoModeInfo::device];

		if (currentInfo.contains(Grabber::currentVideoModeInfo::resolution))
			currentVideoMode = currentInfo[Grabber::currentVideoModeInfo::resolution];

		emit StateChanged(currentDevice, currentVideoMode);
	}
}

