#pragma once

#include <utils/PixelFormat.h>
#include <base/Grabber.h>
#include <grabber/windows/MF/MFWorker.h>
#include <utils/Components.h>

class  MFCallback;
struct IMFSourceReader;

class MFGrabber : public Grabber
{
	Q_OBJECT

public:

	MFGrabber(const QString& device, const QString& configurationPath);

	~MFGrabber();

	void receive_image(const void* frameImageBuffer, int size, QString message);

	int  grabFrame(Image<ColorRgb>&);

	void setHdrToneMappingEnabled(int mode) override;

public slots:

	bool start() override;

	void stop() override;

	void newWorkerFrameHandler(unsigned int workerIndex, Image<ColorRgb> image, quint64 sourceCount, qint64 _frameBegin) override;

	void newWorkerFrameErrorHandler(unsigned int workerIndex, QString error, quint64 sourceCount) override;

private:
	QString GetSharedLut();

	void enumerateMFdevices(bool silent);

	void loadLutFile(PixelFormat color = PixelFormat::NO_CHANGE);

	void getMFdevices();

	bool init() override;

	void uninit() override;

	void close_device();


	bool init_device(QString selectedDeviceName, DevicePropertiesItem props);

	void uninit_device();

	void start_capturing();

	void stop_capturing();

	bool process_image(const void* frameImageBuffer, int size);

	QString FormatRes(int w, int h, QString format);

	QString FormatFrame(int fr);

	QString identify_format(const GUID format, PixelFormat& pixelformat);

private:

	bool			_isMF;
	MFCallback*		_sourceReaderCB;
	MFWorkerManager	_MFWorkerManager;
	IMFSourceReader* _sourceReader;
};
