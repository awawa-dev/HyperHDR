#pragma once

#include <base/SystemWrapper.h>
#include <grabber/linux/amlogic/AmlogicGrabber.h>

class AmlogicWrapper : public SystemWrapper
{
	Q_OBJECT

public:
	AmlogicWrapper(const QString& device, const QString& configurationPath);
	bool isActivated(bool forced) override;

protected:
	QString getGrabberInfo() override;


private:
	AmlogicGrabber _grabber;
};
