#pragma once

#include <hyperhdrbase/GrabberWrapper.h>
#include <grabber/AVFGrabber.h>

class AVFWrapper : public GrabberWrapper
{
	Q_OBJECT

public:
	AVFWrapper(const QString& device, const QString& configurationPath);

private:
	AVFGrabber _grabber;
};
