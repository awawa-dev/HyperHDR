#include <QMetaType>
#include <grabber/X11Wrapper.h>


X11Wrapper::X11Wrapper(const QString &device,
		const QString & configurationPath )
	: SystemWrapper("X11_SYSTEM:"+device.left(14), &_grabber)
	, _grabber(device, configurationPath)
{	
	qRegisterMetaType<Image<ColorRgb>>("Image<ColorRgb>");
	connect(&_grabber, &Grabber::newFrame, this, &SystemWrapper::newFrame, Qt::DirectConnection);
	connect(&_grabber, &Grabber::readError, this, &SystemWrapper::readError, Qt::DirectConnection);
}

