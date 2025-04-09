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


class AccessManager;


class PipewireGrabber : public Grabber
{
	Q_OBJECT

public:

	PipewireGrabber(const QString& device, const QString& configurationPath);

	~PipewireGrabber();

	void setHdrToneMappingEnabled(int mode) override;

	void setCropping(unsigned cropLeft, unsigned cropRight, unsigned cropTop, unsigned cropBottom) override;

	bool hasPipewire(bool force);

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
	
	bool init_device(int _display);

	QString maskToken(const QString& token) const;
		
private:
	QString					_configurationPath;
	QTimer					_timer;

	void*					_library;
	int						_actualDisplay;
	bool					_isActive;
	bool					_storedToken;
	bool					_versionCheck;
	std::shared_ptr<AccessManager> _accessManager;
};
