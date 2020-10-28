#include <QMetaType>

#include <grabber/QTCWrapper.h>

// qt
#include <QTimer>

QTCWrapper::QTCWrapper(const QString &device,
		unsigned grabWidth,
		unsigned grabHeight,
		unsigned fps,
		unsigned input,
		VideoStandard videoStandard,
		PixelFormat pixelFormat,		
		const QString & configurationPath )
	: GrabberWrapper("V4L2:QTC:"+device, &_grabber, grabWidth, grabHeight, 10)
	, _grabber(device,
			grabWidth,
			grabHeight,
			fps,
			input,
			videoStandard,
			pixelFormat,
			configurationPath)
{
	_ggrabber = &_grabber;

	// register the image type
	qRegisterMetaType<Image<ColorRgb>>("Image<ColorRgb>");

	// Handle the image in the captured thread using a direct connection
	connect(&_grabber, &QTCGrabber::newFrame, this, &QTCWrapper::newFrame, Qt::DirectConnection);
	connect(&_grabber, &QTCGrabber::readError, this, &QTCWrapper::readError, Qt::DirectConnection);
}

QTCWrapper::~QTCWrapper()
{
	stop();
}

bool QTCWrapper::start()
{
	return ( _grabber.start() && GrabberWrapper::start());
}

void QTCWrapper::stop()
{
	_grabber.stop();
	GrabberWrapper::stop();
}

void QTCWrapper::setSignalThreshold(double redSignalThreshold, double greenSignalThreshold, double blueSignalThreshold)
{
	_grabber.setSignalThreshold( redSignalThreshold, greenSignalThreshold, blueSignalThreshold, 50);
}

void QTCWrapper::setCropping(unsigned cropLeft, unsigned cropRight, unsigned cropTop, unsigned cropBottom)
{
	_grabber.setCropping(cropLeft, cropRight, cropTop, cropBottom);
}

void QTCWrapper::setSignalDetectionOffset(double verticalMin, double horizontalMin, double verticalMax, double horizontalMax)
{
	_grabber.setSignalDetectionOffset(verticalMin, horizontalMin, verticalMax, horizontalMax);
}

void QTCWrapper::newFrame(const Image<ColorRgb> &image)
{
	emit systemImage(_grabberName, image);
}

void QTCWrapper::readError(const char* err)
{
	Error(_log, "stop grabber, because reading device failed. (%s)", err);
	stop();
}

void QTCWrapper::action()
{
	// dummy as v4l get notifications from stream
}

void QTCWrapper::setSignalDetectionEnable(bool enable)
{
	_grabber.setSignalDetectionEnable(enable);
}

bool QTCWrapper::getSignalDetectionEnable() const
{
	return _grabber.getSignalDetectionEnabled();
}

void QTCWrapper::setCecDetectionEnable(bool enable)
{
	_grabber.setCecDetectionEnable(enable);
}

bool QTCWrapper::getCecDetectionEnable() const
{
	return _grabber.getCecDetectionEnabled();
}

void QTCWrapper::setDeviceVideoStandard(const QString& device, VideoStandard videoStandard)
{
	_grabber.setDeviceVideoStandard(device, videoStandard);
}



void QTCWrapper::setHdrToneMappingEnabled(int mode)
{
	_grabber.setHdrToneMappingEnabled(mode);
}

void QTCWrapper::setFpsSoftwareDecimation(int decimation)
{
	_grabber.setFpsSoftwareDecimation(decimation);
}

void QTCWrapper::setEncoding(QString enc)
{
	_grabber.setEncoding(enc);
}

void QTCWrapper::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
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
			obj["blueSignalThreshold"].toDouble(0.0)/100.0);
		_grabber.setDeviceVideoStandard(
			obj["device"].toString("auto"),
			parseVideoStandard(obj["standard"].toString("no-change")));
			
		_grabber.setEncoding(obj["v4l2Encoding"].toString("NONE"));
	}
}
