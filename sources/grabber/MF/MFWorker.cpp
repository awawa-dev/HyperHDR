/* MFWorker.cpp
*
*  MIT License
*
*  Copyright (c) 2021 awawa-dev
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
#include <base/HyperHdrIManager.h>

#include <QDirIterator>
#include <QFileInfo>

#include <grabber/MFWorker.h>



std::atomic<bool> MFWorker::_isActive(false);

MFWorkerManager::MFWorkerManager() :
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

MFWorkerManager::~MFWorkerManager()
{
	MFWorker::_isActive = false;

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

void MFWorkerManager::Start()
{
	MFWorker::_isActive = true;
}

void MFWorkerManager::InitWorkers()
{
	if (workersCount >= 1)
	{
		workers = new MFWorker*[workersCount];

		for (unsigned i = 0; i < workersCount; i++)
		{
			workers[i] = new MFWorker();
		}
	}
}

void MFWorkerManager::Stop()
{
	MFWorker::_isActive = false;

	if (workers != nullptr)
	{
		for (unsigned i = 0; i < workersCount; i++)
			if (workers[i] != nullptr)
			{
				workers[i]->wait();
			}
	}
}

bool MFWorkerManager::isActive()
{
	return MFWorker::_isActive;
}

MFWorker::MFWorker() :
	_decompress(nullptr),
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

MFWorker::~MFWorker()
{
	if (_decompress != nullptr)
		tjDestroy(_decompress);

	if (_localData != NULL)
	{
		free(_localData);
		_localData = NULL;
		_localDataSize = 0;
	}
}

void MFWorker::setup(unsigned int __workerIndex, PixelFormat __pixelFormat,
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

void MFWorker::run()
{
	runMe();
}

void MFWorker::runMe()
{
	if (_isActive && _width > 0 && _height > 0)
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
				FrameDecoder::processQImage(
					_localData, _width, _height, _lineLength, _pixelFormat, _lutBuffer, image);

				emit newFrame(_workerIndex, image, _currentFrame, _frameBegin);

			}
			else
			{
				int outputWidth = (_width - _cropLeft - _cropRight);
				int outputHeight = (_height - _cropTop - _cropBottom);
				Image<ColorRgb> image(outputWidth, outputHeight);

				FrameDecoder::processImage(
					_cropLeft, _cropRight, _cropTop, _cropBottom,
					_localData, _width, _height, _lineLength, _pixelFormat, _lutBuffer, image);

				emit newFrame(_workerIndex, image, _currentFrame, _frameBegin);
			}
		}
	}
}

void MFWorker::startOnThisThread()
{
	runMe();
}

bool MFWorker::isBusy()
{
	bool temp = false;

	if (_isBusy.compare_exchange_strong(temp, true))
		return false;
	else
		return true;
}

void MFWorker::noBusy()
{
	_isBusy = false;
}


void MFWorker::process_image_jpg_mt()
{
	if (_decompress == nullptr)
		_decompress = tjInitDecompress();

	if (tjDecompressHeader2(_decompress, const_cast<uint8_t*>(_localData), _size, &_width, &_height, &_subsamp) != 0)
	{
		if (tjGetErrorCode(_decompress) == TJERR_FATAL)
		{
			QString info = QString(tjGetErrorStr());
			emit newFrameError(_workerIndex, info, _currentFrame);
			return;
		}
	}

	tjscalingfactor sca{ 1, 2 };
	Image<ColorRgb> srcImage((_qframe) ? TJSCALED(_width, sca) : _width,
		(_qframe) ? TJSCALED(_height, sca) : _height);
	_width = srcImage.width();
	_height = srcImage.height();

	if (tjDecompress2(_decompress, const_cast<uint8_t*>(_localData), _size, (uint8_t*)srcImage.memptr(), _width, 0, _height, TJPF_RGB, TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE) != 0)
	{
		if (tjGetErrorCode(_decompress) == TJERR_FATAL)
		{
			QString info = QString(tjGetErrorStr());
			emit newFrameError(_workerIndex, info, _currentFrame);
			return;
		}
	}

	// got image, process it	
	if (!(_cropLeft > 0 || _cropTop > 0 || _cropBottom > 0 || _cropRight > 0))
	{
		// apply LUT	
		FrameDecoder::applyLUT((unsigned char*)srcImage.memptr(), srcImage.width(), srcImage.height(), _lutBuffer, _hdrToneMappingEnabled);;

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

		for (unsigned int y = 0; y < destImage.height(); y++)
		{
			unsigned char* source = (unsigned char*)srcImage.memptr() + (static_cast<size_t>(y) + _cropTop) * srcImage.width() * 3 + static_cast<size_t>(_cropLeft) * 3;
			unsigned char* dest = (unsigned char*)destImage.memptr() + static_cast<size_t>(y) * destImage.width() * 3;
			memcpy(dest, source, static_cast<size_t>(destImage.width()) * 3);
		}

		// apply LUT	
		FrameDecoder::applyLUT((unsigned char*)destImage.memptr(), destImage.width(), destImage.height(), _lutBuffer, _hdrToneMappingEnabled);

		// exit		
		emit newFrame(_workerIndex, destImage, _currentFrame, _frameBegin);
	}
}


