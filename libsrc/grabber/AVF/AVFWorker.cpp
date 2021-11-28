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
#include <sys/stat.h>
#include <sys/types.h>

#include <hyperhdrbase/HyperHdrInstance.h>
#include <hyperhdrbase/HyperHdrIManager.h>

#include <QDirIterator>
#include <QFileInfo>

#include "grabber/AVFWorker.h"



volatile bool	AVFWorker::_isActive = false;

AVFWorkerManager::AVFWorkerManager():	
	workers(nullptr)	
{
	int select = QThread::idealThreadCount();

	if (select >= 2 && select <= 3)
		select = 2;
	else if (select > 3 && select <= 5)
		select = 3;
	else if (select > 5)
		select = 4;

	workersCount = std::max(select, 1);
}

AVFWorkerManager::~AVFWorkerManager()
{
	if (workers != nullptr)
	{
		for (unsigned i = 0; i < workersCount; i++)
			if (workers[i] != nullptr)
			{
				workers[i]->deleteLater();
				workers[i] = nullptr;
			}
		delete[] workers;
		workers = nullptr;
	}
}

void AVFWorkerManager::Start()
{
	AVFWorker::_isActive = true;
}

void AVFWorkerManager::InitWorkers()
{	
	if (workersCount>=1)
	{
		workers = new AVFWorker*[workersCount];
					
		for (unsigned i=0;i<workersCount;i++)
		{								
			workers[i] = new AVFWorker();
		}
	}
}

void AVFWorkerManager::Stop()
{
	AVFWorker::_isActive =  false;

	if (workers != nullptr)
	{
		for(unsigned i=0; i < workersCount; i++)
			if (workers[i]!=nullptr)
			{
				workers[i]->wait();				
			}
	}
}

bool AVFWorkerManager::isActive()
{
	return AVFWorker::_isActive;
}

AVFWorker::AVFWorker():
	_isBusy(false),
	_semaphore(1),
	_workerIndex(0),
	_pixelFormat(PixelFormat::NO_CHANGE),
	_localData(nullptr),
	_localDataSize(0),
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

AVFWorker::~AVFWorker()
{	
	if (_localData != NULL)
	{
		free(_localData);
		_localData = NULL;
		_localDataSize = 0;
	}
}

void AVFWorker::setup(unsigned int __workerIndex, PixelFormat __pixelFormat,
	uint8_t* __sharedData, int __size, int __width, int __height, int __lineLength,
	uint __cropLeft, uint  __cropTop, uint __cropBottom, uint __cropRight,
	quint64 __currentFrame, qint64 __frameBegin,
	int __hdrToneMappingEnabled, uint8_t* __lutBuffer, bool __qframe)
{
	_workerIndex = __workerIndex;
	_lineLength = __lineLength;
	_pixelFormat = __pixelFormat;
	_size = __size;
	_width = __width;
	_height = __height;
	_cropLeft = __cropLeft;
	_cropTop = __cropTop;
	_cropBottom = __cropBottom;
	_cropRight = __cropRight;
	_currentFrame = __currentFrame;
	_frameBegin = __frameBegin;
	_hdrToneMappingEnabled = __hdrToneMappingEnabled;
	_lutBuffer = __lutBuffer;
	_qframe = __qframe;

	if (__size > _localDataSize)
	{
		if (_localData != NULL)
		{
			free(_localData);
			_localData = NULL;
			_localDataSize = 0;
		}
		_localData = (uint8_t*)malloc((size_t)__size + 1);
		_localDataSize = __size;
	}

	if (_localData != NULL)
		memcpy(_localData, __sharedData, __size);
}


void AVFWorker::run()
{
	runMe();	
}

void AVFWorker::runMe()
{
	if (_isActive && _width > 0 && _height >0)
	{			
		if (_qframe)
		{
			Image<ColorRgb> image(_width >> 1, _height >> 1);
			ImageResampler::processQImage(					
				_localData, _width, _height, _lineLength, _pixelFormat, _lutBuffer, image);

			emit newFrame(_workerIndex, image, _currentFrame, _frameBegin);

		}
		else
		{
			int outputWidth = (_width - _cropLeft - _cropRight);
			int outputHeight = (_height - _cropTop - _cropBottom);
			Image<ColorRgb> image(outputWidth, outputHeight);

			ImageResampler::processImage(
				_cropLeft, _cropRight, _cropTop, _cropBottom,
				_localData, _width, _height, _lineLength, _pixelFormat, _lutBuffer, image);

			emit newFrame(_workerIndex, image, _currentFrame, _frameBegin);
		}		
	}		
}

void AVFWorker::startOnThisThread()
{	
	runMe();
}

bool AVFWorker::isBusy()
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

void AVFWorker::noBusy()
{	
	_semaphore.acquire();
	_isBusy = false;
	_semaphore.release();
}

