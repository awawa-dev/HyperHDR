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

// util includes
#include <utils/PixelFormat.h>
#include <hyperion/Grabber.h>
#include <grabber/VideoStandard.h>
#include <utils/Components.h>
#include <cec/CECEvent.h>

// general JPEG decoder includes
#ifdef HAVE_JPEG_DECODER
	#include <QImage>
	#include <QColor>
#endif

// System JPEG decoder
#ifdef HAVE_JPEG
	#include <jpeglib.h>
	#include <csetjmp>
#endif

// TurboJPEG decoder
#ifdef HAVE_TURBO_JPEG
	#include <turbojpeg.h>
#endif

/// MT worker for V4L2 devices
class V4L2WorkerManager;
class V4L2Worker : public  QThread
{
	Q_OBJECT	
	friend class V4L2WorkerManager;
public:	
	void setup(uint8_t * _data, int _size,int __width, int __height,int __subsamp, 
		   int __pixelDecimation, unsigned  __cropLeft, unsigned  __cropTop, 
		   unsigned __cropBottom, unsigned __cropRight,int __currentFrame, 
		   bool __hdrToneMappingEnabled,unsigned char* _lutBuffer);	
	void run();
	
	V4L2Worker();
	~V4L2Worker();	
signals:
    	void newFrame(Image<ColorRgb> data, unsigned int sourceCount);	
    	void newFrameError(QString,unsigned int);
//    	void finished();	   
public slots:
	void process_image_jpg_mt();
private:			
	static	volatile bool	isActive;	
	tjhandle 	_decompress;

	uint8_t	*data;
	int		size;
	int		_width;
	int		_height;
	int		_subsamp;
	int		_pixelDecimation;
	unsigned	_cropLeft;
	unsigned	_cropTop;
	unsigned	_cropBottom;
	unsigned	_cropRight;
	int		_currentFrame;
	bool 		_hdrToneMappingEnabled;
	unsigned char*	lutBuffer;
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
