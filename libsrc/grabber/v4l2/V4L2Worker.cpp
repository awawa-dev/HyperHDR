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

#include <hyperhdrbase/HyperHdrInstance.h>
#include <hyperhdrbase/HyperHdrIManager.h>

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
		_semaphore(1),
		_workerIndex(0),
		_pixelFormat(PixelFormat::NO_CHANGE),
		_sharedData(nullptr),			
		_size(0),
		_width(0),
		_height(0),
		_lineLength(0),
		_subsamp(0),
		_cropLeft(0),
		_cropTop(0),
		_cropBottom(0),
		_cropRight(0),
		_currentFrame(0),
		_frameBegin(0),
		_hdrToneMappingEnabled(0),
		_lutBuffer(nullptr),
		_qframe(false)
{
	
}

V4L2Worker::~V4L2Worker()
{
	if (_decompress == nullptr)
		tjDestroy(_decompress);
}

void V4L2Worker::setup(unsigned int __workerIndex, v4l2_buffer* __v4l2Buf, PixelFormat __pixelFormat, 
			uint8_t * __sharedData, int __size,int __width, int __height, int __lineLength,
			uint __cropLeft,  uint  __cropTop, uint __cropBottom, uint __cropRight,
			quint64 __currentFrame, qint64 __frameBegin,
			int __hdrToneMappingEnabled, uint8_t* __lutBuffer, bool __qframe)
{
	_workerIndex = __workerIndex;  
	memcpy(&_v4l2Buf, __v4l2Buf, sizeof (v4l2_buffer));
	_lineLength   = __lineLength;
	_pixelFormat  = __pixelFormat;
	_sharedData	  = __sharedData;
	_size		  = __size;
	_width		  = __width;
	_height		  = __height;
	_cropLeft	  = __cropLeft;
	_cropTop	  = __cropTop;
	_cropBottom   = __cropBottom;
	_cropRight	  = __cropRight;
	_currentFrame = __currentFrame;
	_frameBegin	  = __frameBegin;
	_hdrToneMappingEnabled = __hdrToneMappingEnabled;
	_lutBuffer	  = __lutBuffer;
	_qframe		  = __qframe;
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
			if (_qframe)
			{
				_cropLeft = _cropRight = _cropTop = _cropBottom = 0;				
			}

			process_image_jpg_mt();								
		}
		else
		{
			if (_qframe)
			{
				Image<ColorRgb> image(_width >> 1, _height >> 1);
				ImageResampler::processQImage(
					_sharedData, _width, _height, _lineLength, _pixelFormat, _lutBuffer, image);

				emit newFrame(_workerIndex, image, _currentFrame, _frameBegin);

			}
			else
			{
				int outputWidth = (_width - _cropLeft - _cropRight);
				int outputHeight = (_height - _cropTop - _cropBottom);

				Image<ColorRgb> image(outputWidth, outputHeight);

				ImageResampler::processImage(
					_cropLeft, _cropRight, _cropTop, _cropBottom,
					_sharedData, _width, _height, _lineLength, _pixelFormat, _lutBuffer, image);

				emit newFrame(_workerIndex, image, _currentFrame, _frameBegin);
			}
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


void V4L2Worker::process_image_jpg_mt()
{	
	if (_decompress == nullptr)	
		_decompress = tjInitDecompress();	
		
	if (tjDecompressHeader2(_decompress, const_cast<uint8_t*>(_sharedData), _size, &_width, &_height, &_subsamp) != 0)
	{	
		QString info = QString(tjGetErrorStr());
		if (info.indexOf("extraneous bytes before marker") < 0)
		{
			emit newFrameError(_workerIndex, info, _currentFrame);
			return;
		}
	}

	tjscalingfactor sca{ 1,2 };
	Image<ColorRgb> srcImage((_qframe) ? TJSCALED(_width, sca) : _width,
		(_qframe) ? TJSCALED(_height, sca) : _height);
	_width = srcImage.width();
	_height = srcImage.height();

	if (tjDecompress2(_decompress, const_cast<uint8_t*>(_sharedData), _size, (uint8_t*)srcImage.memptr(), _width, 0, _height, TJPF_RGB, TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE) != 0)
	{		
		QString info = QString(tjGetErrorStr());
		if (info.indexOf("extraneous bytes before marker") < 0)
		{
			emit newFrameError(_workerIndex, info, _currentFrame);
			return;
		}
	}
	
	
	// got image, process it	
	if ( !(_cropLeft > 0 || _cropTop > 0 || _cropBottom > 0 || _cropRight > 0))
	{
		// apply LUT	
		ImageResampler::applyLUT((uint8_t*)srcImage.memptr(), srcImage.width(), srcImage.height(), _lutBuffer, _hdrToneMappingEnabled);
    		
    	// exit
    	emit newFrame(_workerIndex, srcImage, _currentFrame, _frameBegin);    		
    }
    else
    {    	
		// calculate the output size
		int outputWidth = (_width - _cropLeft - _cropRight);
		int outputHeight = (_height - _cropTop - _cropBottom);
		
		if (outputWidth <= 0 || outputHeight <= 0)
		{
			QString info = QString("Invalid cropping");
			emit newFrameError(_workerIndex, info, _currentFrame);
			return;	
		}
		
		Image<ColorRgb> destImage(outputWidth, outputHeight);
		
		for (uint32_t y = 0; y < destImage.height(); y++)
		{
			uint8_t* source = (uint8_t*)srcImage.memptr() + (static_cast<size_t>(y) + _cropTop) * srcImage.width() * 3 + static_cast<size_t>(_cropLeft) * 3;
			uint8_t* dest = (uint8_t*)destImage.memptr() + static_cast<size_t>(y) * destImage.width() * 3;
			memcpy(dest, source, static_cast<size_t>(destImage.width()) * 3);
		}
		
		// apply LUT	
		ImageResampler::applyLUT((uint8_t*)destImage.memptr(), destImage.width(), destImage.height(), _lutBuffer, _hdrToneMappingEnabled);
    		
    	// exit
		emit newFrame(_workerIndex, destImage, _currentFrame, _frameBegin);
	}
}


