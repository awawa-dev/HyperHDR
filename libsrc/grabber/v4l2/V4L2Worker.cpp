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
	workersCount = QThread::idealThreadCount();
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
#ifdef HAVE_TURBO_JPEG	
	if (_decompress == nullptr)
		tjDestroy(_decompress);
#endif	
}

void V4L2Worker::setup(uint8_t * _data, int _size,int __width, int __height,int __subsamp, 
		   int __pixelDecimation, unsigned  __cropLeft, unsigned  __cropTop, 
		   unsigned __cropBottom, unsigned __cropRight,int __currentFrame, quint64	__frameBegin,
		   int __hdrToneMappingEnabled,unsigned char* _lutBuffer)
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
	_frameBegin = __frameBegin;
	_hdrToneMappingEnabled = __hdrToneMappingEnabled;
	lutBuffer = _lutBuffer;	
}

void V4L2Worker::run()
{
	#ifdef HAVE_JPEG_DECODER
	if (isActive)
		process_image_jpg_mt();	
	#endif	
	
	delete data;
	data = nullptr;	
}

void V4L2Worker::startOnThisThread()
{	
	this->run();
}


#ifdef HAVE_JPEG_DECODER
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
			emit newFrameError(info,_currentFrame);
			return;
		}

		jpeg_mem_src(_decompress, const_cast<uint8_t*>(data), size);

		if (jpeg_read_header(_decompress, (bool) TRUE) != JPEG_HEADER_OK)
		{
			jpeg_abort_decompress(_decompress);
			jpeg_destroy_decompress(_decompress);
			delete _decompress;
			delete _error;
			
			QString info = QString("JPG: header failed");
			emit newFrameError(info,_currentFrame);
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
			emit newFrameError(info,_currentFrame);
			return;
		}

		if (_decompress->out_color_components != 3)
		{
			jpeg_abort_decompress(_decompress);
			jpeg_destroy_decompress(_decompress);
			delete _decompress;
			delete _error;
			
			QString info = QString("JPG: incorrent color space");
			emit newFrameError(info,_currentFrame);
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
			emit newFrameError(info,_currentFrame);
			return;	
		}
	#endif
	#ifdef HAVE_TURBO_JPEG		
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
	#endif	
	
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
    		if (_hdrToneMappingEnabled == 2)
    		{
    			// border mode
	    		unsigned int width = imageFrame.width();
			unsigned int height = imageFrame.height();    		
	    		unsigned int sizeX= (width * 10)/100;
	    		unsigned int sizeY= (height *17)/100;		
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
		else		
		{
			// fullscreen
			unsigned char* source = imageFrame.bits();
			unsigned char* dest = source + totalBytes;
	    		while (source < dest)
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
    	if (imageFrame.width()%4)
    	{
    		// fix array aligment
    		for (unsigned int y=0; y<imageFrame.height(); y++)
    		{
			unsigned char* source = imageFrame.scanLine(y);
			unsigned char* dest = (unsigned char*)image.memptr()+y*image.width()*3;    		
			memcpy(dest,source,imageFrame.width()*3);
    		}
    		
    	}
	else
		image.copy(imageFrame.bits(),totalBytes);			
			
	// exit
	emit newFrame(image,_currentFrame, _frameBegin);				
}
#endif

