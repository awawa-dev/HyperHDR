#pragma once

#include <base/GrabberWrapper.h>
#include <grabber/linux/v4l2/V4L2Grabber.h>

class V4L2Wrapper : public GrabberWrapper
{
	Q_OBJECT

public:
	V4L2Wrapper(const QString& device, const QString& configurationPath);
};
