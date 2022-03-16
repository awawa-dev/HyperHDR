#pragma once

#include <base/SystemWrapper.h>
#include <grabber/X11Grabber.h>

class X11Wrapper : public SystemWrapper
{
	Q_OBJECT

public:
	X11Wrapper(const QString& device, const QString& configurationPath);

private:
	X11Grabber _grabber;
};
