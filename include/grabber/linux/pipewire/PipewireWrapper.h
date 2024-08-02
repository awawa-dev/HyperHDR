#pragma once

#include <base/SystemWrapper.h>
#include <grabber/linux/pipewire/PipewireGrabber.h>

class PipewireWrapper : public SystemWrapper
{
	Q_OBJECT

public:
	PipewireWrapper(const QString& device, const QString& configurationPath);
	bool isActivated(bool forced) override;

public slots:
	void stateChanged(bool state) override;

protected:
	QString getGrabberInfo() override;
	

private:
	PipewireGrabber _grabber;
};
