#pragma once

#include <Guiddef.h>

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
#include <hyperhdrbase/Grabber.h>
#include <grabber/MFWorker.h>
#include <utils/Components.h>

// general JPEG decoder includes
#include <QImage>
#include <QColor>
#include <turbojpeg.h>

class  MFCallback;
struct IMFSourceReader;

class MFGrabber : public Grabber
{
	Q_OBJECT

public:

	typedef struct
	{
		const GUID		format_id;
		const QString	format_name;
		PixelFormat		pixel;
	} VideoFormat;

	MFGrabber(const QString& device, const QString& configurationPath);

	~MFGrabber();

	void receive_image(const void* frameImageBuffer, int size, QString message);

	int  grabFrame(Image<ColorRgb> &);	

	void setHdrToneMappingEnabled(int mode) override;

public slots:

	bool start() override;

	void stop() override;

	void newWorkerFrame(unsigned int workerIndex, Image<ColorRgb> image, quint64 sourceCount, qint64 _frameBegin) override;

	void newWorkerFrameError(unsigned int workerIndex, QString error, quint64 sourceCount) override;

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

	bool				_isMF;
	MFCallback*			_sourceReaderCB;	
	QString				_configurationPath;				
	MFWorkerManager		_MFWorkerManager;	
	IMFSourceReader*	_sourceReader;
};
