#include <QMetaType>
#include <grabber/macOsWrapper.h>


macOsWrapper::macOsWrapper(const QString &device,
		const QString & configurationPath )
	: SystemWrapper("MACOS_SYSTEM:"+device.left(14), &_grabber)
	, _grabber(device, configurationPath)
{	
	qRegisterMetaType<Image<ColorRgb>>("Image<ColorRgb>");
	connect(&_grabber, &Grabber::newFrame, this, &SystemWrapper::newFrame, Qt::DirectConnection);
	connect(&_grabber, &Grabber::readError, this, &SystemWrapper::readError, Qt::DirectConnection);
}

