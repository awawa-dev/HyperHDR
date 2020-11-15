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
#include <hyperion/Grabber.h>
#include <utils/Components.h>


// TurboJPEG decoder
#ifdef HAVE_TURBO_JPEG
	#include <QImage>
	#include <QColor>
	#include <turbojpeg.h>
#endif

/// MT worker for V4L2 devices
class QTCWorkerManager;
class QTCWorker : public  QThread
{
	Q_OBJECT	
	friend class QTCWorkerManager;
	
	public:	
		void setup(
				unsigned int _workerIndex, 
				PixelFormat __pixelFormat,
				uint8_t * _sharedData, int _size,int __width, int __height, int __lineLength,
				int __subsamp, 
				unsigned  __cropLeft, unsigned  __cropTop, 
				unsigned __cropBottom, unsigned __cropRight,int __currentFrame, quint64 __frameBegin,
				int __hdrToneMappingEnabled,unsigned char* _lutBuffer);	
		void startOnThisThread();
		void run();
		
		bool isBusy();
		void noBusy();
		
		QTCWorker();
		~QTCWorker();	
	signals:
	    	void newFrame(unsigned int workerIndex, const Image<ColorRgb>& data, unsigned int sourceCount, quint64 _frameBegin);	
	    	void newFrameError(unsigned int workerIndex, QString,unsigned int sourceCount);   
	    					
	private:
		void runMe();
		void process_image_jpg_mt();																			
		void applyLUT(unsigned char* _source, unsigned int width ,unsigned int height);
		
	#ifdef HAVE_TURBO_JPEG		
		tjhandle 	_decompress;			
	#endif		
		
	static	volatile bool	_isActive;
		volatile bool  _isBusy;
		QSemaphore	_semaphore;
		unsigned int 	_workerIndex;				
		PixelFormat	_pixelFormat;		
		uint8_t* _localData;
		int 	 _localDataSize;
		int		 _size;
		int		 _width;
		int		 _height;
		int		 _lineLength;
		int		 _subsamp;
		unsigned	_cropLeft;
		unsigned	_cropTop;
		unsigned	_cropBottom;
		unsigned	_cropRight;
		int		_currentFrame;
		uint64_t	_frameBegin;
		uint8_t	_hdrToneMappingEnabled;
		unsigned char*	lutBuffer;
};

class QTCWorkerManager : public  QObject
{
	Q_OBJECT

public:
	QTCWorkerManager();
	~QTCWorkerManager();
	
	bool isActive();
	void InitWorkers();
	void Stop();
	void Start();
	
	// MT workers
	unsigned int	workersCount;
	QTCWorker**		workers;
};
