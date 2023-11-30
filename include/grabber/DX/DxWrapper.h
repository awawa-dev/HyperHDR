#pragma once

#include <base/SystemWrapper.h>
#include <grabber/DX/DxGrabber.h>

class DxWrapper : public SystemWrapper
{
	Q_OBJECT

public:
	DxWrapper(const QString& device, const QString& configurationPath);

private:
	DxGrabber _grabber;
};
