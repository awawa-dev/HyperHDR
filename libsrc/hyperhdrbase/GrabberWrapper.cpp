
#include <hyperhdrbase/GrabberWrapper.h>
#include <hyperhdrbase/Grabber.h>
#include <HyperhdrConfig.h>

// utils includes
#include <utils/GlobalSignals.h>
#include <utils/QStringUtils.h>
#include <hyperhdrbase/HyperHdrIManager.h>

// qt
#include <QTimer>

GrabberWrapper* GrabberWrapper::instance = nullptr;

GrabberWrapper::GrabberWrapper(const QString& grabberName, Grabber* ggrabber)
	: _grabberName(grabberName)
	, _log(Logger::getInstance(grabberName))
	, _configLoaded(false)
	, _grabber(ggrabber)
	, _cecHdrStart(0)
	, _cecHdrStop(0)
	, _autoResume(false)
{
	GrabberWrapper::instance = this;

	// connect the image forwarding
	
	connect(this, &GrabberWrapper::systemImage, GlobalSignals::getInstance(), &GlobalSignals::setVideoImage);

	// listen for source requests
	connect(GlobalSignals::getInstance(), &GlobalSignals::requestSource, this, &GrabberWrapper::handleSourceRequest);

	connect(this, &GrabberWrapper::cecKeyPressed, this, &GrabberWrapper::cecKeyPressedHandler);
}

void GrabberWrapper::setCecStartStop(int cecHdrStart, int cecHdrStop)
{
	_cecHdrStart = cecHdrStart;
	_cecHdrStop = cecHdrStop;
	
	Debug(_log, "CEC keycode. Start: %i, stop: %i", _cecHdrStart, _cecHdrStop);
}

QJsonDocument GrabberWrapper::startCalibration()
{
	if (_grabber != nullptr)
		return _grabber->startCalibration();
	else
		return QJsonDocument();
}

QJsonDocument GrabberWrapper::stopCalibration()
{
	if (_grabber != nullptr)
		return _grabber->stopCalibration();
	else
		return QJsonDocument();
}

QJsonDocument GrabberWrapper::getCalibrationInfo()
{
	if (_grabber != nullptr)
		return _grabber->getCalibrationInfo();
	else
		return QJsonDocument();
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
	emit systemImage(_grabberName, image);
}

void GrabberWrapper::readError(const char* err)
{
	Error(_log, "Grabber signals error (%s)", err);
}

GrabberWrapper::~GrabberWrapper()
{
	Debug(_log,"Closing grabber: %s", QSTRING_CSTR(_grabberName));
}


QString GrabberWrapper::getActive() const
{
	return _grabberName;
}

QStringList GrabberWrapper::availableGrabbers()
{
	QStringList grabbers;

	#ifdef ENABLE_V4L2
	grabbers << "Video capturing (V4L2)";
	#endif

	#ifdef ENABLE_MF
	grabbers << "Video capturing (MF)";
	#endif

	#ifdef ENABLE_AVF
	grabbers << "Video capturing (AVF)";
	#endif	

	return grabbers;
}

void GrabberWrapper::handleSourceRequest(hyperhdr::Components component, int hyperhdrInd, bool listen)
{
	if(component == hyperhdr::Components::COMP_VIDEOGRABBER)
	{
		if(listen && !GRABBER_VIDEO_CLIENTS.contains(hyperhdrInd))
			GRABBER_VIDEO_CLIENTS.append(hyperhdrInd);
		else if (!listen)
			GRABBER_VIDEO_CLIENTS.removeOne(hyperhdrInd);

		if (GRABBER_VIDEO_CLIENTS.empty())
		{
			stop();

			//update state
			QString currentDevice = "", currentVideoMode = "";			
			emit StateChanged(currentDevice, currentVideoMode);
		}
		else
		{
			start();

			// update state
			QString currentDevice = "", currentVideoMode = "";
			auto currentInfo = getVideoCurrentMode();

			if (currentInfo.contains(Grabber::currentVideoModeInfo::device))
				currentDevice = currentInfo[Grabber::currentVideoModeInfo::device];

			if (currentInfo.contains(Grabber::currentVideoModeInfo::resolution))
				currentVideoMode = currentInfo[Grabber::currentVideoModeInfo::resolution];

			emit StateChanged(currentDevice, currentVideoMode);
		}
	}
}

void GrabberWrapper::tryStart()
{
	// verify start condition
	if((!GRABBER_VIDEO_CLIENTS.empty()))
	{
		start();
	}
}

QStringList GrabberWrapper::getVideoDevices() const
{
	if (_grabber != nullptr)
		return _grabber->getVideoDevices();
	else
		return QStringList();
}

QString GrabberWrapper::getVideoDeviceName(const QString& devicePath) const
{
	if (_grabber != nullptr)
		return _grabber->getVideoDeviceName(devicePath);
	else
		return QString();
}

QMap<Grabber::currentVideoModeInfo, QString> GrabberWrapper::getVideoCurrentMode() const
{
	if (_grabber != nullptr)
		return _grabber->getVideoCurrentMode();
	else
		return QMap<Grabber::currentVideoModeInfo, QString>();
}

QMultiMap<QString, int> GrabberWrapper::getVideoDeviceInputs(const QString& devicePath) const
{
	if (_grabber != nullptr)
		return _grabber->getVideoDeviceInputs(devicePath);
	else
		return QMultiMap<QString, int>();
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

void GrabberWrapper::setBrightnessContrastSaturationHue(int brightness, int contrast, int saturation, int hue)
{
	if (_grabber != nullptr)
		_grabber->setBrightnessContrastSaturationHue(brightness, contrast, saturation, hue);
}

void GrabberWrapper::setQFrameDecimation(int setQframe)
{
	if (_grabber != nullptr)
		_grabber->setQFrameDecimation(setQframe);
}

QMap<Grabber::VideoControls, int> GrabberWrapper::getVideoDeviceControls(const QString& devicePath)
{
	if (_grabber != nullptr)
		return _grabber->getVideoDeviceControls(devicePath);
	else
		return QMap<Grabber::VideoControls, int>();
}

QList<Grabber::DevicePropertiesItem> GrabberWrapper::getVideoDeviceModesFullInfo(const QString& devicePath)
{
	if (_grabber != nullptr)
		return _grabber->getVideoDeviceModesFullInfo(devicePath);
	else
		return QList<Grabber::DevicePropertiesItem>();
}

DetectionAutomatic::calibrationPoint GrabberWrapper::parsePoint(int width, int height, QJsonObject element, bool &ok)
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
		QMetaObject::invokeMethod(_grabber, "revive", Qt::QueuedConnection);
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
				Debug(_log, "Auto resume is: %s",(_autoResume)?"enabled":"disabled");
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
			setHdrToneMappingEnabled(obj["hdrToneMapping"].toBool(false)? obj["hdrToneMappingMode"].toInt(1): 0);

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

