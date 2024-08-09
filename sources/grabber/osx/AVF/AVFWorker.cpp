/* AVFWorker.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2024 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/HyperHDR
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
*/

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

#include <base/HyperHdrInstance.h>

#include <QDirIterator>
#include <QFileInfo>

#include <grabber/osx/AVF/AVFWorker.h>
#include <utils/GlobalSignals.h>


std::atomic<bool> AVFWorker::_isActive(false);

AVFWorkerManager::AVFWorkerManager() :
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
	AVFWorker::_isActive = false;

	if (workers != nullptr)
	{
		for (unsigned i = 0; i < workersCount; i++)
			if (workers[i] != nullptr)
			{
				workers[i]->wait();
				delete workers[i];
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
	if (workersCount >= 1)
	{
		workers = new AVFWorker*[workersCount];

		for (unsigned i = 0; i < workersCount; i++)
		{
			workers[i] = new AVFWorker();
		}
	}
}

void AVFWorkerManager::Stop()
{
	AVFWorker::_isActive = false;

	if (workers != nullptr)
	{
		for (unsigned i = 0; i < workersCount; i++)
			if (workers[i] != nullptr)
			{
				workers[i]->wait();
			}
	}
}

bool AVFWorkerManager::isActive()
{
	return AVFWorker::_isActive;
}

AVFWorker::AVFWorker() :
	_isBusy(false),
	_semaphore(1),
	_workerIndex(0),
	_pixelFormat(PixelFormat::NO_CHANGE),
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
}

void AVFWorker::setup(unsigned int __workerIndex, PixelFormat __pixelFormat,
	uint8_t* __sharedData, int __size, int __width, int __height, int __lineLength,
	uint __cropLeft, uint  __cropTop, uint __cropBottom, uint __cropRight,
	quint64 __currentFrame, qint64 __frameBegin,
	int __hdrToneMappingEnabled, uint8_t* __lutBuffer, bool __qframe, bool __directAccess, QString __deviceName)
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
	_directAccess = __directAccess;
	_deviceName = __deviceName;

	_localBuffer.resize((size_t)__size + 1);

	memcpy(_localBuffer.data(), __sharedData, __size);
}


void AVFWorker::run()
{
	runMe();
}

void AVFWorker::runMe()
{
	if (_isActive && _width > 0 && _height > 0)
	{
		if (_qframe)
		{
			Image<ColorRgb> image(_width >> 1, _height >> 1);
			FrameDecoder::processQImage(
				_localBuffer.data(), _width, _height, _lineLength, _pixelFormat, _lutBuffer, image);

			image.setBufferCacheSize();
			if (!_directAccess)
				emit SignalNewFrame(_workerIndex, image, _currentFrame, _frameBegin);
			else
			{
				emit GlobalSignals::getInstance()->SignalNewVideoImage(_deviceName, image);
				emit SignalNewFrame(_workerIndex, Image<ColorRgb>(), _currentFrame, _frameBegin);
			}

		}
		else
		{
			int outputWidth = (_width - _cropLeft - _cropRight);
			int outputHeight = (_height - _cropTop - _cropBottom);
			Image<ColorRgb> image(outputWidth, outputHeight);

			FrameDecoder::processImage(
				_cropLeft, _cropRight, _cropTop, _cropBottom,
				_localBuffer.data(), nullptr, _width, _height, _lineLength, _pixelFormat, _lutBuffer, image);

			image.setBufferCacheSize();
			if (!_directAccess)
				emit SignalNewFrame(_workerIndex, image, _currentFrame, _frameBegin);
			else
			{
				emit GlobalSignals::getInstance()->SignalNewVideoImage(_deviceName, image);
				emit SignalNewFrame(_workerIndex, Image<ColorRgb>(), _currentFrame, _frameBegin);
			}
		}
	}
}

void AVFWorker::startOnThisThread()
{
	runMe();
}

bool AVFWorker::isBusy()
{
	bool temp = false;

	if (_isBusy.compare_exchange_strong(temp, true))
		return false;
	else
		return true;
}

void AVFWorker::noBusy()
{
	_isBusy = false;
}

