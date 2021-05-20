#include <QMetaType>
#include <grabber/AVFWrapper.h>


AVFWrapper::AVFWrapper(const QString& device,
	const QString& configurationPath)
	: GrabberWrapper("macOS AVF:" + device.left(14), &_grabber)
	, _grabber(device, configurationPath)
{
	qRegisterMetaType<Image<ColorRgb>>("Image<ColorRgb>");
	connect(&_grabber, &Grabber::newFrame, this, &GrabberWrapper::newFrame, Qt::DirectConnection);
	connect(&_grabber, &Grabber::readError, this, &GrabberWrapper::readError, Qt::DirectConnection);
}

