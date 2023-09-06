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
#include <grabber/V4L2Worker.h>
#include <utils/Components.h>

// general JPEG decoder includes
#include <QImage>
#include <QColor>
#include <turbojpeg.h>

class V4L2Grabber : public Grabber
{
	Q_OBJECT

public:
	struct HyperHdrFormat
	{
		__u32		v4l2Format;
		PixelFormat innerFormat;
	};

	V4L2Grabber(const QString& device, const QString& configurationPath);

	~V4L2Grabber();

	int  grabFrame(Image<ColorRgb>&);

	void setHdrToneMappingEnabled(int mode) override;

public slots:

	bool start() override;

	void stop() override;

	void newWorkerFrame(unsigned int workerIndex, Image<ColorRgb> image, quint64 sourceCount, qint64 _frameBegin) override;

	void newWorkerFrameError(unsigned int workerIndex, QString error, quint64 sourceCount) override;

private slots:
	int read_frame();

private:
	QString GetSharedLut();

	void enumerateV4L2devices(bool silent);

	void loadLutFile(PixelFormat color = PixelFormat::NO_CHANGE);

	void getV4L2devices();

	bool init() override;

	void uninit() override;

	void close_device();

	bool init_mmap();

	bool init_device(QString selectedDeviceName, DevicePropertiesItem props);

	void uninit_device();

	void start_capturing();

	void stop_capturing();

	bool process_image(v4l2_buffer* buf, const void* frameImageBuffer, int size);

	int xioctl(int request, void* arg);

	int xioctl(int fileDescriptor, int request, void* arg);

	void throw_exception(const QString& error);

	void throw_errno_exception(const QString& error);

	QString formatRes(int w, int h, QString format);

	QString formatFrame(int fr);

	PixelFormat identifyFormat(__u32 format);

	bool getControl(int _fd, __u32 controlId, long& minVal, long& maxVal, long& defVal);

	bool setControl(__u32 controlId, __s32 newValue);

private:

	struct buffer
	{
		void* start;
		size_t  length;
	};

	int                 _fileDescriptor;
	std::vector<buffer> _buffers;
	QSocketNotifier*	_streamNotifier;	
	V4L2WorkerManager   _V4L2WorkerManager;
};
