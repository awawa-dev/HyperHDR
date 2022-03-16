#pragma once

#include <base/SystemWrapper.h>
#include <grabber/macOsGrabber.h>

class macOsWrapper : public SystemWrapper
{
	Q_OBJECT

public:
	macOsWrapper(const QString& device, const QString& configurationPath);

private:
	macOsGrabber _grabber;
};
