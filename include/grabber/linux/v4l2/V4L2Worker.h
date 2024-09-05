#pragma once

#ifndef PCH_ENABLED
	#include <QThread>

	#include <vector>
	#include <atomic>
#endif

#include <utils/PixelFormat.h>
#include <base/Grabber.h>
#include <utils/Components.h>

#include <turbojpeg.h>

class V4L2WorkerManager;
struct v4l2_buffer;
class V4L2Worker;

class V4L2Worker : public  QThread
{
	Q_OBJECT
		friend class V4L2WorkerManager;

public:
	void setup(
		unsigned int _workerIndex, v4l2_buffer* __v4l2Buf,
		PixelFormat __pixelFormat,
		uint8_t*	__sharedData,
		int			__size, int __width, int __height, int __lineLength,
		unsigned	__cropLeft, unsigned  __cropTop,
		unsigned	__cropBottom, unsigned __cropRight,
		quint64		__currentFrame, qint64 __frameBegin,
		int			__hdrToneMappingEnabled, uint8_t* __lutBuffer,
		bool		__qframe, bool __directAccess, QString __deviceName);

	void startOnThisThread();
	void run() override;

	v4l2_buffer* GetV4L2Buffer();
	bool isBusy();
	void noBusy();

	V4L2Worker();
	~V4L2Worker();

signals:
	void SignalNewFrame(unsigned int workerIndex, Image<ColorRgb> data, quint64 sourceCount, qint64 _frameBegin);
	void SignalNewFrameError(unsigned int workerIndex, QString, quint64 sourceCount);

private:
	void runMe();
	void process_image_jpg_mt();

	tjhandle 	_decompress;

	static	std::atomic<bool> _isActive;
	std::atomic<bool>   _isBusy;
	unsigned int 	    _workerIndex;
	struct v4l2_buffer  _v4l2Buf;
	PixelFormat			_pixelFormat;
	uint8_t*    _sharedData;
	int			_size;
	int			_width;
	int			_height;
	int			_lineLength;
	int			_subsamp;
	uint		_cropLeft;
	uint		_cropTop;
	uint		_cropBottom;
	uint		_cropRight;
	quint64		_currentFrame;
	qint64		_frameBegin;
	uint8_t	    _hdrToneMappingEnabled;
	uint8_t*    _lutBuffer;
	bool		_qframe;
	bool		_directAccess;
	QString		_deviceName;
};

class V4L2WorkerManager : public  QObject
{
	Q_OBJECT

public:
	V4L2WorkerManager();
	~V4L2WorkerManager();

	bool isActive();
	void InitWorkers();
	void Stop();
	void Start();

	// MT workers
	unsigned int	workersCount;
	V4L2Worker**	workers;
};
