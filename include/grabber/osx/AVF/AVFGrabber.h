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

// util includes
#include <utils/PixelFormat.h>
#include <base/Grabber.h>
#include <grabber/osx/AVF/AVFWorker.h>
#include <utils/Components.h>




class AVFGrabber : public Grabber
{
	Q_OBJECT

public:

	typedef struct
	{
		const QString	formatName;
		PixelFormat		pixel;
	} format_t;

	AVFGrabber(const QString& device, const QString& configurationPath);

	~AVFGrabber();

	void receive_image(const void* frameImageBuffer, int size, QString message);

	int  grabFrame(Image<ColorRgb>&);

	void setHdrToneMappingEnabled(int mode) override;

public slots:

	bool start() override;

	void stop() override;

	void newWorkerFrameHandler(unsigned int workerIndex, Image<ColorRgb> image, quint64 sourceCount, qint64 _frameBegin) override;

	void newWorkerFrameErrorHandler(unsigned int workerIndex, QString error, quint64 sourceCount) override;

	bool getPermission();

private:
	QString GetSharedLut();

	void enumerateAVFdevices(bool silent);

	void loadLutFile(PixelFormat color = PixelFormat::NO_CHANGE);

	void enumerateAVFdevices();

	bool init() override;

	void uninit() override;

	void close_device();


	bool init_device(QString selectedDeviceName, DevicePropertiesItem props);

	void uninit_device();

	void start_capturing();

	void stop_capturing();

	bool process_image(const void* frameImageBuffer, int size);

	PixelFormat identifyFormat(QString format);

	QString FormatRes(int w, int h, QString format);

	QString FormatFrame(int fr);

	QString getIdOfCodec(uint32_t cc4);

private:
	bool				_isAVF;
	AVFWorkerManager	_AVFWorkerManager;
	bool		_permission;
};

