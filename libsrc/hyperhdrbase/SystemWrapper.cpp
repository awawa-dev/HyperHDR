
#include <hyperhdrbase/SystemWrapper.h>
#include <hyperhdrbase/Grabber.h>
#include <HyperhdrConfig.h>

// utils includes
#include <utils/GlobalSignals.h>
#include <utils/QStringUtils.h>
#include <hyperhdrbase/HyperHdrIManager.h>
// qt
#include <QTimer>

SystemWrapper* SystemWrapper::instance = nullptr;

SystemWrapper::SystemWrapper(const QString& grabberName, Grabber* ggrabber)
	: _grabberName(grabberName)
	, _log(Logger::getInstance(grabberName))
	, _configLoaded(false)
	, _grabber(ggrabber)
{
	SystemWrapper::instance = this;

	// connect the image forwarding
	
	connect(this, &SystemWrapper::systemImage, GlobalSignals::getInstance(), &GlobalSignals::setSystemImage);

	// listen for source requests
	connect(GlobalSignals::getInstance(), &GlobalSignals::requestSource, this, &SystemWrapper::handleSourceRequest);
}

void SystemWrapper::newFrame(const Image<ColorRgb>& image)
{
	emit systemImage(_grabberName, image);
}

void SystemWrapper::readError(const char* err)
{
	Error(_log, "Grabber signals error (%s)", err);
}

SystemWrapper::~SystemWrapper()
{
	Debug(_log,"Closing grabber: %s", QSTRING_CSTR(_grabberName));
}


QString SystemWrapper::getActive() const
{
	return _grabberName;
}

QStringList SystemWrapper::availableGrabbers()
{
	QStringList grabbers;

	#ifdef ENABLE_X11
	grabbers << "System capturing (X11)";
	#endif

	#ifdef ENABLE_DX
	grabbers << "System capturing (DX)";
	#endif

	#ifdef ENABLE_MAC_SYSTEM
	grabbers << "System capturing (macOS)";
	#endif	

	return grabbers;
}

void SystemWrapper::handleSourceRequest(hyperhdr::Components component, int hyperhdrInd, bool listen)
{
	if(component == hyperhdr::Components::COMP_SYSTEMGRABBER)
	{
		if(listen && !GRABBER_SYSTEM_CLIENTS.contains(hyperhdrInd))
			GRABBER_SYSTEM_CLIENTS.append(hyperhdrInd);
		else if (!listen)
			GRABBER_SYSTEM_CLIENTS.removeOne(hyperhdrInd);

		if(GRABBER_SYSTEM_CLIENTS.empty())
			stop();
		else
			start();
	}
}

void SystemWrapper::tryStart()
{
	// verify start condition
	if((!GRABBER_SYSTEM_CLIENTS.empty()))
	{
		start();
	}
}

QStringList SystemWrapper::getVideoDevices() const
{
	if (_grabber != nullptr)
		return _grabber->getVideoDevices();
	else
		return QStringList();
}

QString SystemWrapper::getVideoDeviceName(const QString& devicePath) const
{
	if (_grabber != nullptr)
		return _grabber->getVideoDeviceName(devicePath);
	else
		return QString();
}

QMap<Grabber::currentVideoModeInfo, QString> SystemWrapper::getVideoCurrentMode() const
{
	if (_grabber != nullptr)
		return _grabber->getVideoCurrentMode();
	else
		return QMap<Grabber::currentVideoModeInfo, QString>();
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

int SystemWrapper::getHdrToneMappingEnabled()
{
	if (_grabber != nullptr)
		return _grabber->getHdrToneMappingEnabled();
	else
		return 0;
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
			setHdrToneMappingEnabled(obj["hdrToneMapping"].toBool(false)? 1: 0);
#endif

			// signal
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

