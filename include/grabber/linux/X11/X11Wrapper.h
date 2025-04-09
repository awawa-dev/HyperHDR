#pragma once

#include <base/SystemWrapper.h>
#include <grabber/linux/X11/X11Grabber.h>

class X11Wrapper : public SystemWrapper
{
	Q_OBJECT

public:
	X11Wrapper(const QString& device, const QString& configurationPath);

	bool isActivated(bool forced) override;

private:
	X11Grabber _grabber;
};
