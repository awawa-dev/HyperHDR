#pragma once

#include <base/SystemWrapper.h>
#include <base/GrabberWrapper.h>

class GrabberHelper : public QObject {
	Q_OBJECT

	QObject* _grabberInstance = nullptr;

signals:
	void SignalCreateGrabber();

public:
	~GrabberHelper();
	void setGrabber(QObject* grabber);
	SystemWrapper* systemWrapper();
	GrabberWrapper* grabberWrapper();

	QSemaphore linker { 0 };
};
