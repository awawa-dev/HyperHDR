#pragma once

// stl includes
#include <vector>
#include <map>

// Qt includes
#include <QObject>
#include <QSocketNotifier>
#include <QRectF>
#include <QMap>
#include <QMultiMap>
#include <QThread>
#include <QSemaphore>

// util includes
#include <utils/PixelFormat.h>
#include <hyperhdrbase/Grabber.h>
#include <utils/Components.h>
#include <linux/videodev2.h>

// general JPEG decoder includes
#include <QImage>
#include <QColor>

// TurboJPEG decoder
#include <turbojpeg.h>


/// MT worker for V4L2 devices
class V4L2WorkerManager;
class V4L2Worker : public  QThread
{
	Q_OBJECT	
	friend class V4L2WorkerManager;
	
	public:	
		void setup(
				unsigned int _workerIndex, v4l2_buffer* __v4l2Buf,
				PixelFormat __pixelFormat,
				uint8_t * _sharedData, int __size,int __width, int __height, int __lineLength,
				int		  __subsamp, 
				unsigned  __cropLeft, unsigned  __cropTop, 
				unsigned  __cropBottom, unsigned __cropRight,int __currentFrame, quint64 __frameBegin,
				int		  __hdrToneMappingEnabled, unsigned char* _lutBuffer, bool __qframe);
		void startOnThisThread();
		void run();
		
		v4l2_buffer* GetV4L2Buffer();
		bool isBusy();
		void noBusy();
		
		V4L2Worker();
		~V4L2Worker();	
	signals:
	    	void newFrame(unsigned int workerIndex, Image<ColorRgb> data, unsigned int sourceCount, quint64 _frameBegin);	
	    	void newFrameError(unsigned int workerIndex, QString,unsigned int sourceCount);   
	    					
	private:
		void runMe();
		void process_image_jpg_mt();																			
	
		tjhandle 	_decompress;

static	volatile bool	   _isActive;
		volatile bool      _isBusy;
		QSemaphore	       _semaphore;
		unsigned int 	   _workerIndex;
		struct v4l2_buffer _v4l2Buf;		
		PixelFormat			_pixelFormat;		
		uint8_t*			sharedData;
		int			_size;
		int			_width;
		int			_height;
		int			_lineLength;
		int			_subsamp;
		unsigned	_cropLeft;
		unsigned	_cropTop;
		unsigned	_cropBottom;
		unsigned	_cropRight;
		int		    _currentFrame;
		uint64_t	_frameBegin;
		uint8_t	    _hdrToneMappingEnabled;
		unsigned char*	lutBuffer;
		bool		_qframe;
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
