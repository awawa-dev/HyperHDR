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
#include <grabber/VideoStandard.h>
#include <utils/Components.h>
#include <cec/CECEvent.h>
#include <linux/videodev2.h>

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
		void setup(
				unsigned int _workerIndex, v4l2_buffer* __v4l2Buf,
				VideoMode __videoMode, PixelFormat __pixelFormat,
				uint8_t * _sharedData, int _size,int __width, int __height, int __lineLength,
				int __subsamp, 
				int __pixelDecimation, unsigned  __cropLeft, unsigned  __cropTop, 
				unsigned __cropBottom, unsigned __cropRight,int __currentFrame, quint64 __frameBegin,
				int __hdrToneMappingEnabled,unsigned char* _lutBuffer);	
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
		void process_image_jpg_mt_turbo();										
		void applyLUT(unsigned char* target, unsigned int width ,unsigned int height, unsigned int bytesPerLine);
	#ifdef HAVE_TURBO_JPEG		
		tjhandle 	_decompress;			
	#endif
	
	#ifdef HAVE_JPEG_DECODER			
	#else
		void*	 	_decompress;		
	#endif
	
	#ifdef HAVE_JPEG
		struct errorManager
		{
			jpeg_error_mgr pub;
			jmp_buf setjmp_buffer;
		};

		static void errorHandler(j_common_ptr cInfo)
		{
			errorManager* mgr = reinterpret_cast<errorManager*>(cInfo->err);
			longjmp(mgr->setjmp_buffer, 1);
		}

		static void outputHandler(j_common_ptr cInfo)
		{
			// Suppress fprintf warnings.
		}

		jpeg_decompress_struct* _decompress;
		errorManager* _error;
	#endif
		
	static	volatile bool	_isActive;
		volatile bool  _isBusy;
		QSemaphore	_semaphore;
		unsigned int 	_workerIndex;
		struct v4l2_buffer _v4l2Buf;
		VideoMode 	_videoMode;	
		PixelFormat	_pixelFormat;		
		uint8_t	*sharedData;
		int		size;
		int		_width;
		int		_height;
		int		_lineLength;
		int		_subsamp;
		int		_pixelDecimation;
		unsigned	_cropLeft;
		unsigned	_cropTop;
		unsigned	_cropBottom;
		unsigned	_cropRight;
		int		_currentFrame;
		uint64_t	_frameBegin;
		uint8_t	_hdrToneMappingEnabled;
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
