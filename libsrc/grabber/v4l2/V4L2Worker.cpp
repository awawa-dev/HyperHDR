#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <thread>
#include <chrono>
#include <time.h>
#include <iomanip>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

#include <hyperion/Hyperion.h>
#include <hyperion/HyperionIManager.h>

#include <QDirIterator>
#include <QFileInfo>

#include "grabber/V4L2Worker.h"



volatile bool	V4L2Worker::_isActive = false;

V4L2WorkerManager::V4L2WorkerManager():	
	workers(nullptr)	
{
	workersCount = std::max(QThread::idealThreadCount(),1);
}

V4L2WorkerManager::~V4L2WorkerManager()
{
	if (workers!=nullptr)
	{
		for(unsigned i=0; i < workersCount; i++)
			if (workers[i]!=nullptr)
			{
				workers[i]->deleteLater();
				workers[i] = nullptr;
			}
		delete[] workers;
		workers = nullptr;
	}	
}

void V4L2WorkerManager::Start()
{
	V4L2Worker::_isActive = true;
}

void V4L2WorkerManager::InitWorkers()
{	
	if (workersCount>=1)
	{
		workers = new V4L2Worker*[workersCount];
					
		for (unsigned i=0;i<workersCount;i++)
		{								
			workers[i] = new V4L2Worker();
		}
	}
}

void V4L2WorkerManager::Stop()
{
	V4L2Worker::_isActive =  false;

	if (workers != nullptr)
	{
		for(unsigned i=0; i < workersCount; i++)
			if (workers[i]!=nullptr)
			{
				workers[i]->quit();
				workers[i]->wait();				
			}
	}
}

bool V4L2WorkerManager::isActive()
{
	return V4L2Worker::_isActive;
}

V4L2Worker::V4L2Worker():
		_decompress(nullptr),
		_isBusy(false),
		_semaphore(1)
{
	
}

V4L2Worker::~V4L2Worker(){
#ifdef HAVE_TURBO_JPEG	
	if (_decompress == nullptr)
		tjDestroy(_decompress);
#endif			
}

void V4L2Worker::setup(unsigned int __workerIndex, v4l2_buffer* __v4l2Buf, VideoMode __videoMode,PixelFormat __pixelFormat, 
			uint8_t * _sharedData, int _size,int __width, int __height, int __lineLength,
			int __subsamp, 
			unsigned  __cropLeft, unsigned  __cropTop, 
			unsigned __cropBottom, unsigned __cropRight,int __currentFrame, quint64	__frameBegin,
			int __hdrToneMappingEnabled,unsigned char* _lutBuffer)
{
	_workerIndex = __workerIndex;  
	memcpy(&_v4l2Buf,__v4l2Buf,sizeof (v4l2_buffer));
	_lineLength = __lineLength;
	_videoMode = __videoMode;
	_pixelFormat = __pixelFormat;
	sharedData = _sharedData;
	size = _size;
	_width = __width;
	_height = __height;
	_subsamp = __subsamp;
	_cropLeft = __cropLeft;
	_cropTop = __cropTop;
	_cropBottom = __cropBottom;
	_cropRight = __cropRight;
	_currentFrame = __currentFrame;
	_frameBegin = __frameBegin;
	_hdrToneMappingEnabled = __hdrToneMappingEnabled;
	lutBuffer = _lutBuffer;	
}

v4l2_buffer* V4L2Worker::GetV4L2Buffer()
{
	return &_v4l2Buf;
}

void V4L2Worker::run()
{
	runMe();	
}

void V4L2Worker::runMe()
{
	if (_isActive)
	{
		if (_pixelFormat == PixelFormat::MJPEG)
		{
		#ifdef HAVE_JPEG_DECODER	
			#ifdef HAVE_TURBO_JPEG		
				if (!(_cropLeft>0 || _cropTop>0 || _cropBottom>0 || _cropRight>0))
					process_image_jpg_mt_turbo();	
				else
			#endif	
				process_image_jpg_mt();					
			
		#endif	
		}
		else
		{
			Image<ColorRgb> image(_width, _height);
			
			ImageResampler::processImage(
				_videoMode,
				_cropLeft, _cropRight, _cropTop, _cropBottom,
				sharedData, _width, _height, _lineLength, _pixelFormat, lutBuffer, image);
				
			emit newFrame(_workerIndex, image, _currentFrame, _frameBegin);		
		}
	}		
}

void V4L2Worker::startOnThisThread()
{	
	runMe();
}

bool V4L2Worker::isBusy()
{	
	bool temp;
	_semaphore.acquire();
	if (_isBusy)
		temp = true;
	else
	{
		temp = false;	
		_isBusy = true;
	}
	_semaphore.release();
	return temp;
}

void V4L2Worker::noBusy()
{	
	_semaphore.acquire();
	_isBusy = false;
	_semaphore.release();
}


void V4L2Worker::process_image_jpg_mt_turbo()
{				
	Image<ColorRgb> image(_width, _height);		
		
	if (_decompress == nullptr)	
		_decompress = tjInitDecompress();	
	
	if (tjDecompressHeader2(_decompress, const_cast<uint8_t*>(sharedData), size, &_width, &_height, &_subsamp) != 0)
	{	
		QString info = QString(tjGetErrorStr());
		emit newFrameError(_workerIndex, info,_currentFrame);				
		return;
	}
			
	if (tjDecompress2(_decompress, const_cast<uint8_t*>(sharedData), size, (unsigned char*)image.memptr(), _width, 0, _height, TJPF_RGB, TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE) != 0)
	{		
		QString info = QString(tjGetErrorStr());
		emit newFrameError(_workerIndex, info,_currentFrame);
		return;
	}

	// apply LUT	
    	applyLUT((unsigned char*)image.memptr(), (unsigned char*)image.memptr(), _width, _height, _width*3);
			
	// exit
	emit newFrame(_workerIndex, image, _currentFrame, _frameBegin);	
}

void V4L2Worker::process_image_jpg_mt()
{				
	
	Image<ColorRgb> image(_width, _height);		
	
	#ifdef HAVE_JPEG
		_decompress = new jpeg_decompress_struct;
		_error = new errorManager;

		_decompress->err = jpeg_std_error(&_error->pub);
		_error->pub.error_exit = &errorHandler;
		_error->pub.output_message = &outputHandler;

		jpeg_create_decompress(_decompress);

		if (setjmp(_error->setjmp_buffer))
		{
			jpeg_abort_decompress(_decompress);
			jpeg_destroy_decompress(_decompress);
			delete _decompress;
			delete _error;
			
			QString info = QString("JPG: buffer failed");
			emit newFrameError(_workerIndex, info,_currentFrame);
			return;
		}

		jpeg_mem_src(_decompress, const_cast<uint8_t*>(sharedData), size);

		if (jpeg_read_header(_decompress, (bool) TRUE) != JPEG_HEADER_OK)
		{
			jpeg_abort_decompress(_decompress);
			jpeg_destroy_decompress(_decompress);
			delete _decompress;
			delete _error;
			
			QString info = QString("JPG: header failed");
			emit newFrameError(_workerIndex, info,_currentFrame);
			return;
		}

		_decompress->scale_num = 1;
		_decompress->scale_denom = 1;
		_decompress->out_color_space = JCS_RGB;
		_decompress->dct_method = JDCT_IFAST;

		if (!jpeg_start_decompress(_decompress))
		{
			jpeg_abort_decompress(_decompress);
			jpeg_destroy_decompress(_decompress);
			delete _decompress;
			delete _error;
			
			QString info = QString("JPG: decompress failed");
			emit newFrameError(_workerIndex, info,_currentFrame);
			return;
		}

		if (_decompress->out_color_components != 3)
		{
			jpeg_abort_decompress(_decompress);
			jpeg_destroy_decompress(_decompress);
			delete _decompress;
			delete _error;
			
			QString info = QString("JPG: incorrent color space");
			emit newFrameError(_workerIndex, info,_currentFrame);
			return;	
		}

		QImage imageFrame = QImage(_decompress->output_width, _decompress->output_height, QImage::Format_RGB888);

		int y = 0;
		while (_decompress->output_scanline < _decompress->output_height)
		{
			uchar *row = imageFrame.scanLine(_decompress->output_scanline);
			jpeg_read_scanlines(_decompress, &row, 1);
			y++;
		}

		jpeg_finish_decompress(_decompress);
		jpeg_destroy_decompress(_decompress);
		delete _decompress;
		delete _error;

		if (_error->pub.num_warnings > 0)
		{
			QString info = QString("JPG library: there was some warnings");
			emit newFrameError(_workerIndex, info,_currentFrame);
			return;	
		}
	#endif
	#ifdef HAVE_TURBO_JPEG		
		if (_decompress == nullptr)	
			_decompress = tjInitDecompress();	
		
		if (tjDecompressHeader2(_decompress, const_cast<uint8_t*>(sharedData), size, &_width, &_height, &_subsamp) != 0)
		{	
			QString info = QString(tjGetErrorStr());
			emit newFrameError(_workerIndex, info,_currentFrame);				
			return;
		}
		
		QImage imageFrame = QImage(_width, _height, QImage::Format_RGB888);
		
		if (tjDecompress2(_decompress, const_cast<uint8_t*>(sharedData), size, imageFrame.bits(), _width, 0, _height, TJPF_RGB, TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE) != 0)
		{		
			QString info = QString(tjGetErrorStr());
			emit newFrameError(_workerIndex, info,_currentFrame);
			return;
		}
	#endif	
	
	if (imageFrame.isNull())
	{
		QString info = QString("Empty frame detected");
		emit newFrameError(_workerIndex, info,_currentFrame);
		return;	
	}

	if (_cropLeft>0 || _cropTop>0 || _cropBottom>0 || _cropRight>0)
	{
		QRect rect(_cropLeft, _cropTop, imageFrame.width() - _cropLeft - _cropRight, imageFrame.height() - _cropTop - _cropBottom);
		imageFrame = imageFrame.copy(rect);
	}	

	if ((image.width() != unsigned(imageFrame.width())) || (image.height() != unsigned(imageFrame.height())))
	{
		image.resize(imageFrame.width(), imageFrame.height());
	}

	// apply LUT
	if (lutBuffer != NULL && _hdrToneMappingEnabled)
		applyLUT((unsigned char*)image.memptr(), imageFrame.bits(), imageFrame.width(), imageFrame.height(), imageFrame.bytesPerLine());
	else
	{	
	    	// bytes are in order of RGB 3 bytes because of TJPF_RGB    	
	    	if (imageFrame.width() % 4)
	    	{    		
	    		// fix array aligment
	    		unsigned int height = (unsigned int)imageFrame.height();  
	    		for (unsigned int y=0; y<height; y++)
	    		{
				unsigned char* source = imageFrame.scanLine(y);
				unsigned char* dest = (unsigned char*)image.memptr()+y*image.width()*3;    		
				memcpy(dest,source,imageFrame.width()*3);
	    		}
	    		
	    	}
		else
			image.copy(imageFrame.bits(), imageFrame.width()*imageFrame.height()*3);			
	}	
		
	// exit
	emit newFrame(_workerIndex, image, _currentFrame, _frameBegin);				
}

#define LUT(dest,source) \
{\
	memcpy(buffer, source, 3); \
	uint32_t ind_lutd = LUT_INDEX(buffer[0],buffer[1],buffer[2]);	\
	memcpy(dest, (void *) &(lutBuffer[ind_lutd]),3); \
	dest += 3;	\
	source += 3;	\
}

void V4L2Worker::applyLUT(unsigned char* _target, unsigned char* _source, unsigned int width ,unsigned int height, unsigned int bytesPerLine)
{	
    	// apply LUT table mapping
    	// bytes are in order of RGB 3 bytes because of TJPF_RGB   
    	uint8_t buffer[3];     	
    	if (lutBuffer != NULL && _hdrToneMappingEnabled)
    	{    		
    		unsigned int sizeX= (width * 10)/100;
    		unsigned int sizeY= (height *15)/100;		
    		
    		for (unsigned int y=0; y<height; y++)
    		{    	
    			unsigned char* startSource = _source + bytesPerLine * y;
    			unsigned char* startDest = _target + width * 3 * y;
    			unsigned char* endDest = startDest + width * 3;
    			
    			if (_hdrToneMappingEnabled != 2 || y<sizeY || y>height-sizeY)
    			{
    				while (startDest < endDest)
		    			LUT(startDest, startSource);
    			}
    			else
    			{
		    		for (unsigned int x=0; x<sizeX; x++)    		
		    			LUT(startDest, startSource);
		    			
		    		startSource += (width-2*sizeX)*3;
		    		startDest += (width-2*sizeX)*3; 
		    		
		    		while (startSource < endDest)
		    			LUT(startDest, startSource);
		    	}
		}
		
    	}
}


