#pragma once

#ifndef PCH_ENABLED
	#include <atomic>
#endif

#include <base/SoundCapture.h>

class WindowsSoundThread : public QThread
{
	Q_OBJECT

	std::atomic<bool> _exitNow{ false };
	Logger* _logger;
	QString _device;
	SoundCapture* _parent;

	void run() override;

public:
	WindowsSoundThread(Logger* logger, QString device, SoundCapture* parent);
	void exitNow();
};

class SoundCaptureWindows : public SoundCapture
{
	Q_OBJECT

public:
	SoundCaptureWindows(const QJsonDocument& effectConfig, QObject* parent = nullptr);
	~SoundCaptureWindows();
	void start() override;
	void stop() override;
private:
	void stopDevice();
	void listDevices();
	WindowsSoundThread* _thread;
};

typedef SoundCaptureWindows SoundGrabber;
