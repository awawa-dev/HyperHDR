#pragma once

#include <base/GrabberWrapper.h>
#include <grabber/windows/MF/MFGrabber.h>

class MFWrapper : public GrabberWrapper
{
	Q_OBJECT

public:
	MFWrapper(const QString& device, const QString& configurationPath);
};
