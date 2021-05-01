#include <QMetaType>

#include <grabber/AVFWrapper.h>

// qt
#include <QTimer>

AVFWrapper::AVFWrapper(const QString &device,
		unsigned grabWidth,
		unsigned grabHeight,
		unsigned fps,
		unsigned input,		
		PixelFormat pixelFormat,		
		const QString & configurationPath )
	: GrabberWrapper("V4L2:macOS AVF:"+device, &_grabber, grabWidth, grabHeight, 10)
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
	connect(&_grabber, &AVFGrabber::newFrame, this, &AVFWrapper::newFrame, Qt::DirectConnection);
	connect(&_grabber, &AVFGrabber::readError, this, &AVFWrapper::readError, Qt::DirectConnection);
}

AVFWrapper::~AVFWrapper()
{
	stop();
}

bool AVFWrapper::start()
{
	return ( _grabber.start() && GrabberWrapper::start());
}

void AVFWrapper::stop()
{
	_grabber.stop();
	GrabberWrapper::stop();
}

void AVFWrapper::setSignalThreshold(double redSignalThreshold, double greenSignalThreshold, double blueSignalThreshold, int noSignalCounterThreshold)
{
	_grabber.setSignalThreshold( redSignalThreshold, greenSignalThreshold, blueSignalThreshold, noSignalCounterThreshold);
}

void AVFWrapper::setCropping(unsigned cropLeft, unsigned cropRight, unsigned cropTop, unsigned cropBottom)
{
	_grabber.setCropping(cropLeft, cropRight, cropTop, cropBottom);
}

void AVFWrapper::setSignalDetectionOffset(double verticalMin, double horizontalMin, double verticalMax, double horizontalMax)
{
	_grabber.setSignalDetectionOffset(verticalMin, horizontalMin, verticalMax, horizontalMax);
}

void AVFWrapper::newFrame(const Image<ColorRgb> &image)
{
	emit systemImage(_grabberName, image);
}

void AVFWrapper::readError(const char* err)
{
	Error(_log, "stop grabber, because reading device failed. (%s)", err);
	stop();
}

void AVFWrapper::action()
{
	// dummy as v4l get notifications from stream
}

void AVFWrapper::setSignalDetectionEnable(bool enable)
{
	_grabber.setSignalDetectionEnable(enable);
}

bool AVFWrapper::getSignalDetectionEnable() const
{
	return _grabber.getSignalDetectionEnabled();
}

void AVFWrapper::setDeviceVideoStandard(const QString& device)
{
	_grabber.setDeviceVideoStandard(device);
}

void AVFWrapper::setHdrToneMappingEnabled(int mode)
{
	_grabber.setHdrToneMappingEnabled(mode);
}

int AVFWrapper::getHdrToneMappingEnabled()
{
	return _grabber.getHdrToneMappingEnabled();
}

void AVFWrapper::setFpsSoftwareDecimation(int decimation)
{
	_grabber.setFpsSoftwareDecimation(decimation);
}

void AVFWrapper::setEncoding(QString enc)
{
	_grabber.setEncoding(enc);
}

void AVFWrapper::setBrightnessContrastSaturationHue(int brightness, int contrast, int saturation, int hue)
{
	_grabber.setBrightnessContrastSaturationHue(brightness, contrast, saturation, hue);
}

void AVFWrapper::setQFrameDecimation(int setQframe)
{
	_grabber.setQFrameDecimation(setQframe);
}

void AVFWrapper::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if(type == settings::type::V4L2 && _grabberName.startsWith("V4L"))
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

		_grabber.setQFrameDecimation(obj["qFrame"].toBool(false));
	}
}
