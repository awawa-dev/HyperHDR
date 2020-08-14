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

bool	V4L2Worker::isActive = false;

V4L2WorkerManager::V4L2WorkerManager():
	workersCount(0),
	workers(NULL)
{

}

V4L2WorkerManager::~V4L2WorkerManager()
{
	if (workers!=NULL)
	{
		for(unsigned i=0; i < workersCount; i++)
			workers[i]->deleteLater();
		delete[] workers;
		workers = NULL;
	}	
}

void V4L2WorkerManager::Start()
{
	V4L2Worker::isActive = true;
}

void V4L2WorkerManager::InitWorkers()
{
	workersCount = QThread::idealThreadCount();
	workers = new V4L2Worker*[workersCount];
				
	for (unsigned i=0;i<workersCount;i++)
	{						
		V4L2Worker *_workerThread = new V4L2Worker();									   
		workers[i] = _workerThread;
	}
}

void V4L2WorkerManager::Stop()
{
	V4L2Worker::isActive =  false;

	if (workers != NULL)
	{
		for(unsigned i=0; i < workersCount; i++)
		{
			workers[i]->quit();
			workers[i]->wait();
		}
	}
}
	
	
V4L2Worker::V4L2Worker()
{
	//_decompress = tjInitDecompress();
}
	
V4L2Worker::~V4L2Worker()
{
	//if (_decompress == nullptr)
	//	tjDestroy(_decompress);	
	//_decompress = nullptr;	
}
		
void V4L2Worker::setup(uint8_t * _data, int _size,int __width, int __height,int __subsamp, 
		   int __pixelDecimation, unsigned  __cropLeft, unsigned  __cropTop, 
		   unsigned __cropBottom, unsigned __cropRight,int __currentFrame, 
		   bool __hdrToneMappingEnabled,unsigned char* _lutBuffer)
{
	data = _data;
	size = _size;
	_width = __width;
	_height = __height;
	_subsamp = __subsamp;
	_pixelDecimation = __pixelDecimation;
	_cropLeft = __cropLeft;
	_cropTop = __cropTop;
	_cropBottom = __cropBottom;
	_cropRight = __cropRight;
	_currentFrame = __currentFrame;
	_hdrToneMappingEnabled = __hdrToneMappingEnabled;
	lutBuffer = _lutBuffer;	
}

void V4L2Worker::run()
{
	if (!isActive)	
		return;

	process_image_jpg_mt();
}
				
void V4L2Worker::process_image_jpg_mt()
{		
	Image<ColorRgb> image(_width, _height);
	tjhandle	_decompress = tjInitDecompress();
	
	if (_decompress == nullptr)
	{
		tjDestroy(_decompress);
		delete data;
		return;
	}
	
	if (tjDecompressHeader2(_decompress, const_cast<uint8_t*>(data), size, &_width, &_height, &_subsamp) != 0)
	{	
		tjDestroy(_decompress);
		delete data;
		return;
	}

	QImage imageFrame = QImage(_width, _height, QImage::Format_RGB888);
	if (tjDecompress2(_decompress, const_cast<uint8_t*>(data), size, imageFrame.bits(), _width, 0, _height, TJPF_RGB, TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE) != 0)
	{		
		tjDestroy(_decompress);
		delete data;
		return;
	}
	
	tjDestroy(_decompress);
	delete data;

	if (imageFrame.isNull())				
		return;	

	if (_cropLeft>0 || _cropTop>0 || _cropBottom>0 || _cropRight>0)
	{
		QRect rect(_cropLeft, _cropTop, imageFrame.width() - _cropLeft - _cropRight, imageFrame.height() - _cropTop - _cropBottom);
		imageFrame = imageFrame.copy(rect);
	}
	if (_pixelDecimation>1)
	{
		imageFrame = imageFrame.scaled(imageFrame.width() / _pixelDecimation, imageFrame.height() / _pixelDecimation,Qt::KeepAspectRatio);
	}

	if ((image.width() != unsigned(imageFrame.width())) || (image.height() != unsigned(imageFrame.height())))
	{
		image.resize(imageFrame.width(), imageFrame.height());
	}


	// prepare to copy buffer	
    	unsigned int totalBytes = (imageFrame.width() * imageFrame.height() * 3);
    	
    	// apply LUT table mapping
    	// bytes are in order of RGB 3 bytes because of TJPF_RGB
    	if (lutBuffer != NULL && _hdrToneMappingEnabled)
    	{
    		unsigned char* source = imageFrame.bits();
    		for (unsigned int i=0;i+3<totalBytes;i+=3)
    		{
    			unsigned char* r = source++;
    			unsigned char* g = source++;
    			unsigned char* b = source++;	    				    			
    			size_t ind_lutd = (
				LUTD_Y_STRIDE(*r) +
				LUTD_U_STRIDE(*g) +
				LUTD_V_STRIDE(*b)
			);
			*r = lutBuffer[ind_lutd + LUTD_C_STRIDE(0)];
			*g = lutBuffer[ind_lutd + LUTD_C_STRIDE(1)];
			*b = lutBuffer[ind_lutd + LUTD_C_STRIDE(2)];
    		}
    	}
    	
    	// bytes are in order of RGB 3 bytes because of TJPF_RGB
	image.copy(imageFrame.bits(),totalBytes);					    			
	
		
	emit newFrame(image,_currentFrame);	
}


