#pragma once

#include <hyperhdrbase/GrabberWrapper.h>
#include <grabber/V4L2Grabber.h>

class V4L2Wrapper : public GrabberWrapper
{
	Q_OBJECT

public:
	V4L2Wrapper(const QString& device, const QString& configurationPath);

private:
	V4L2Grabber _grabber;
};
