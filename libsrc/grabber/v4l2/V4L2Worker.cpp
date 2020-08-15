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

volatile bool	V4L2Worker::isActive = false;

V4L2WorkerManager::V4L2WorkerManager():
	workersCount(0),
	workers(nullptr)	
{

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
	V4L2Worker::isActive = true;
}

void V4L2WorkerManager::InitWorkers()
{
	
	workersCount = QThread::idealThreadCount();
	workers = new V4L2Worker*[workersCount];
				
	for (unsigned i=0;i<workersCount;i++)
	{								
		workers[i] = new V4L2Worker();
	}
}

void V4L2WorkerManager::Stop()
{
	V4L2Worker::isActive =  false;

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
	return V4L2Worker::isActive;
}
	
V4L2Worker::V4L2Worker():
		_decompress(nullptr)
{
	
}

V4L2Worker::~V4L2Worker(){
	if (_decompress == nullptr)
		tjDestroy(_decompress);

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
	if (isActive)
		process_image_jpg_mt();		
	
	delete data;
	data = nullptr;
	
	//emit finished(this);	
}

void V4L2Worker::process_image_jpg_mt()
{		
	Image<ColorRgb> image(_width, _height);	
		
	if (_decompress == nullptr)	
		_decompress = tjInitDecompress();	
	
	if (tjDecompressHeader2(_decompress, const_cast<uint8_t*>(data), size, &_width, &_height, &_subsamp) != 0)
	{	
		QString info = QString(tjGetErrorStr());
		emit newFrameError(info,_currentFrame);				
		return;
	}

	QImage imageFrame = QImage(_width, _height, QImage::Format_RGB888);
	if (tjDecompress2(_decompress, const_cast<uint8_t*>(data), size, imageFrame.bits(), _width, 0, _height, TJPF_RGB, TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE) != 0)
	{		
		QString info = QString(tjGetErrorStr());
		emit newFrameError(info,_currentFrame);
		return;
	}
	
	if (imageFrame.isNull())
	{
		QString info = QString("Empty frame detected");
		emit newFrameError(info,_currentFrame);
		return;	
	}

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
    		unsigned int width = imageFrame.width();
		unsigned int height = imageFrame.height();    		
    		unsigned int sizeX= (width * 10)/100;
    		unsigned int sizeY= (height *15)/100;		
    		unsigned char* source = imageFrame.bits();
    		
    		for (unsigned int y=0; y<height; y++)
    		{    	
    			unsigned char* orgSource = source;		
	    		for (unsigned int x=0; x<width; x++)    		
				if (x>sizeX && x<width-sizeX && y>sizeY && y<height-sizeY)
				{
					x = width - sizeX;
					source = orgSource + (x + 1) * 3;
				}
				else
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
    	}
    	
    	// bytes are in order of RGB 3 bytes because of TJPF_RGB
	image.copy(imageFrame.bits(),totalBytes);					    			
			
	// exit
	emit newFrame(image,_currentFrame);		
}


