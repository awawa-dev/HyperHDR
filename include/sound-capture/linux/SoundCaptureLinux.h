#pragma once

#ifndef PCH_ENABLED
	#include <atomic>
#endif

#include <base/SoundCapture.h>

class AlsaWorkerThread : public QThread
{
	Q_OBJECT
		
		std::atomic<bool> _exitNow{ false };
		Logger* _logger;
		QString _device;
		SoundCapture* _parent;

		void run() override;

	public:
		AlsaWorkerThread(Logger* logger, QString device, SoundCapture* parent);
		void exitNow();		
};

class SoundCaptureLinux : public SoundCapture
{
	Q_OBJECT

	public:
		SoundCaptureLinux(const QJsonDocument& effectConfig, QObject* parent = nullptr);
		~SoundCaptureLinux();
		void start() override;
		void stop() override;
	private:
		void listDevices();
		AlsaWorkerThread* _thread;
};

typedef SoundCaptureLinux SoundGrabber;
