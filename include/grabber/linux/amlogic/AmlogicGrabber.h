#pragma once

// stl includes
#include <vector>
#include <map>
#include <chrono>

// Qt includes
#include <QObject>
#include <QSocketNotifier>
#include <QRectF>
#include <QMap>
#include <QMultiMap>
#include <QTimer>
#include <QSemaphore>

// util includes
#include <utils/PixelFormat.h>
#include <base/Grabber.h>
#include <utils/Components.h>


//AML
#include <grabber/linux/amlogic/Amvideocap.h>


class AmlogicGrabber : public Grabber
{
	Q_OBJECT

public:

	AmlogicGrabber(const QString& device, const QString& configurationPath);

	~AmlogicGrabber();

	void setHdrToneMappingEnabled(int mode) override;

	void setCropping(unsigned cropLeft, unsigned cropRight, unsigned cropTop, unsigned cropBottom) override;

	bool isActivated();

	void stateChanged(bool state);

private slots:

	void grabFrame();

public slots:

	bool start() override;

	void stop() override;

	void newWorkerFrameHandler(unsigned int workerIndex, Image<ColorRgb> image, quint64 sourceCount, qint64 _frameBegin) override {};

	void newWorkerFrameErrorHandler(unsigned int workerIndex, QString error, quint64 sourceCount) override {};

private:
	QString GetSharedLut();

	void enumerateDevices(bool silent);

	void loadLutFile(PixelFormat color = PixelFormat::NO_CHANGE);

	void getDevices();

	bool init() override;

	void uninit() override;

	void resetVariables();


	//AMLOGIC
	bool isVideoPlayingAML();
	void closeDeviceAML(int& fd);
	bool openDeviceAML(int& fd, const char* dev);
	bool initAmlogic();
	bool stopAmlogic();
	bool grabFrameAmlogic();
	bool grabFrameFramebuffer();

	MemoryBuffer<uint8_t> _amlFrame;
	MemoryBuffer<uint8_t> _lastValidFrame;
	int _captureDev;
	int _videoDev;
	bool _usingAmlogic;
	bool _messageShow;

private:
	QString		_configurationPath;
	QTimer		_timer;
	QSemaphore	_semaphore;
	int			_handle;
};
