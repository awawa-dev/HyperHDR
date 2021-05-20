#pragma once

#include <hyperhdrbase/SystemWrapper.h>
#include <grabber/DxGrabber.h>

class DxWrapper : public SystemWrapper
{
	Q_OBJECT

public:
	DxWrapper(const QString& device, const QString& configurationPath);

private:
	DxGrabber _grabber;
};
