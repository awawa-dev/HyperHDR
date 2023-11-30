#pragma once

#include <base/SystemWrapper.h>
#include <grabber/framebuffer/FrameBufGrabber.h>

class FrameBufWrapper : public SystemWrapper
{
	Q_OBJECT

public:
	FrameBufWrapper(const QString& device, const QString& configurationPath);
	bool isActivated(bool forced) override;

protected:
	QString getGrabberInfo() override;
	

private:
	FrameBufGrabber _grabber;
};
