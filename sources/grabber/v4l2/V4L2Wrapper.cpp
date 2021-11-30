#include <QMetaType>
#include <grabber/V4L2Wrapper.h>

V4L2Wrapper::V4L2Wrapper(const QString& device,
	const QString& configurationPath)
	: GrabberWrapper("V4L2:" + device.left(14), &_grabber)
	, _grabber(device, configurationPath)
{
	qRegisterMetaType<Image<ColorRgb>>("Image<ColorRgb>");
	connect(&_grabber, &V4L2Grabber::newFrame, this, &GrabberWrapper::newFrame, Qt::DirectConnection);
	connect(&_grabber, &V4L2Grabber::readError, this, &GrabberWrapper::readError, Qt::DirectConnection);
}
