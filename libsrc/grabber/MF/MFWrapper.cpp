#include <QMetaType>

#include <grabber/MFWrapper.h>

// qt
#include <QTimer>

MFWrapper::MFWrapper(const QString &device,
		unsigned grabWidth,
		unsigned grabHeight,
		unsigned fps,
		unsigned input,		
		PixelFormat pixelFormat,		
		const QString & configurationPath )
	: GrabberWrapper("V4L2:Media Foundation:"+device, &_grabber, grabWidth, grabHeight, 10)
	, _grabber(device,
			grabWidth,
			grabHeight,
			fps,
			input,			
			pixelFormat,
			configurationPath)
{
	_ggrabber = &_grabber;

	// register the image type
	qRegisterMetaType<Image<ColorRgb>>("Image<ColorRgb>");

	// Handle the image in the captured thread using a direct connection
	connect(&_grabber, &MFGrabber::newFrame, this, &MFWrapper::newFrame, Qt::DirectConnection);
	connect(&_grabber, &MFGrabber::readError, this, &MFWrapper::readError, Qt::DirectConnection);
}

MFWrapper::~MFWrapper()
{
	stop();
}

bool MFWrapper::start()
{
	return ( _grabber.start() && GrabberWrapper::start());
}

void MFWrapper::stop()
{
	_grabber.stop();
	GrabberWrapper::stop();
}

void MFWrapper::setSignalThreshold(double redSignalThreshold, double greenSignalThreshold, double blueSignalThreshold, int noSignalCounterThreshold)
{
	_grabber.setSignalThreshold( redSignalThreshold, greenSignalThreshold, blueSignalThreshold, noSignalCounterThreshold);
}

void MFWrapper::setCropping(unsigned cropLeft, unsigned cropRight, unsigned cropTop, unsigned cropBottom)
{
	_grabber.setCropping(cropLeft, cropRight, cropTop, cropBottom);
}

void MFWrapper::setSignalDetectionOffset(double verticalMin, double horizontalMin, double verticalMax, double horizontalMax)
{
	_grabber.setSignalDetectionOffset(verticalMin, horizontalMin, verticalMax, horizontalMax);
}

void MFWrapper::newFrame(const Image<ColorRgb> &image)
{
	emit systemImage(_grabberName, image);
}

void MFWrapper::readError(const char* err)
{
	Error(_log, "stop grabber, because reading device failed. (%s)", err);
	stop();
}

void MFWrapper::action()
{
	// dummy as v4l get notifications from stream
}

void MFWrapper::setSignalDetectionEnable(bool enable)
{
	_grabber.setSignalDetectionEnable(enable);
}

bool MFWrapper::getSignalDetectionEnable() const
{
	return _grabber.getSignalDetectionEnabled();
}

void MFWrapper::setCecDetectionEnable(bool enable)
{
	_grabber.setCecDetectionEnable(enable);
}

bool MFWrapper::getCecDetectionEnable() const
{
	return _grabber.getCecDetectionEnabled();
}

void MFWrapper::setDeviceVideoStandard(const QString& device)
{
	_grabber.setDeviceVideoStandard(device);
}



void MFWrapper::setHdrToneMappingEnabled(int mode)
{
	_grabber.setHdrToneMappingEnabled(mode);
}

int MFWrapper::getHdrToneMappingEnabled()
{
	return _grabber.getHdrToneMappingEnabled();
}

void MFWrapper::setFpsSoftwareDecimation(int decimation)
{
	_grabber.setFpsSoftwareDecimation(decimation);
}

void MFWrapper::setEncoding(QString enc)
{
	_grabber.setEncoding(enc);
}

void MFWrapper::setBrightnessContrastSaturationHue(int brightness, int contrast, int saturation, int hue)
{
	_grabber.setBrightnessContrastSaturationHue(brightness, contrast, saturation, hue);
}


void MFWrapper::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if(type == settings::V4L2 && _grabberName.startsWith("V4L"))
	{
		// extract settings
		const QJsonObject& obj = config.object();		

		// crop for v4l
		_grabber.setCropping(
			obj["cropLeft"].toInt(0),
			obj["cropRight"].toInt(0),
			obj["cropTop"].toInt(0),
			obj["cropBottom"].toInt(0));

		// device input
		_grabber.setInput(obj["input"].toInt(-1));

		// device resolution
		_grabber.setWidthHeight(obj["width"].toInt(0), obj["height"].toInt(0));

		// device framerate
		_grabber.setFramerate(obj["fps"].toInt(15));
		
		_grabber.setBrightnessContrastSaturationHue(obj["hardware_brightness"].toInt(0), 
													obj["hardware_contrast"].toInt(0),
													obj["hardware_saturation"].toInt(0),
													obj["hardware_hue"].toInt(0));

		// CEC Standby
		_grabber.setCecDetectionEnable(obj["cecDetection"].toBool(true));

		// HDR tone mapping
		if (!obj["hdrToneMapping"].toBool(false))	
		{
			_grabber.setHdrToneMappingEnabled(0);
		}
		else
		{
			_grabber.setHdrToneMappingEnabled(obj["hdrToneMappingMode"].toInt(1));
		}
		emit HdrChanged(_grabber.getHdrToneMappingEnabled());
		
		// software frame skipping
		_grabber.setFpsSoftwareDecimation(obj["fpsSoftwareDecimation"].toInt(1));
		
		_grabber.setSignalDetectionEnable(obj["signalDetection"].toBool(true));
		_grabber.setSignalDetectionOffset(
			obj["sDHOffsetMin"].toDouble(0.25),
			obj["sDVOffsetMin"].toDouble(0.25),
			obj["sDHOffsetMax"].toDouble(0.75),
			obj["sDVOffsetMax"].toDouble(0.75));
		_grabber.setSignalThreshold(
			obj["redSignalThreshold"].toDouble(0.0)/100.0,
			obj["greenSignalThreshold"].toDouble(0.0)/100.0,
			obj["blueSignalThreshold"].toDouble(0.0)/100.0,
			obj["noSignalCounterThreshold"].toInt(50) );
		_grabber.setDeviceVideoStandard(obj["device"].toString("auto"));
			
		_grabber.setEncoding(obj["v4l2Encoding"].toString("NONE"));
	}
}
