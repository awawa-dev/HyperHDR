/* MFWorker.cpp
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

#include <QDirIterator>
#include <QFileInfo>

#include <base/HyperHdrInstance.h>
#include <grabber/windows/MF/MFWorker.h>
#include <utils/GlobalSignals.h>


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

MFWorker::~MFWorker()
{
	if (_decompress != nullptr)
		tjDestroy(_decompress);	
}

void MFWorker::setup(unsigned int __workerIndex, PixelFormat __pixelFormat,
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
			process_image_jpg_mt();
		}
		else
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

	if (tjDecompressHeader2(_decompress, _localBuffer.data(), _size, &_width, &_height, &_subsamp) != 0 &&
		tjGetErrorCode(_decompress) == TJERR_FATAL)
	{
        emit SignalNewFrameError(_workerIndex, QString(tjGetErrorStr()), _currentFrame);
		return;
	}
	
	if ((_subsamp != TJSAMP_422 && _subsamp != TJSAMP_420) && _hdrToneMappingEnabled > 0)
	{
		emit SignalNewFrameError(_workerIndex, QString("%1: %2").arg(UNSUPPORTED_DECODER).arg(_subsamp), _currentFrame);
		return;
	}

	tjscalingfactor sca{ 1, 2 };

	_width = (_qframe) ? TJSCALED(_width, sca) : _width;
	_height = (_qframe) ? TJSCALED(_height, sca) : _height;

	Image<ColorRgb> image(_width - _cropLeft - _cropRight, _height - _cropTop - _cropBottom);

	if (_hdrToneMappingEnabled > 0)
	{
		size_t yuvSize = tjBufSizeYUV2(_width, 2, _height, _subsamp);
		MemoryBuffer<uint8_t> jpgBuffer(yuvSize);

		if (tjDecompressToYUV2(_decompress, _localBuffer.data(), _size, jpgBuffer.data(), _width, 2, _height, TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE) != 0 &&
			tjGetErrorCode(_decompress) == TJERR_FATAL)
			{
				emit SignalNewFrameError(_workerIndex, QString(tjGetErrorStr()), _currentFrame);
				return;
			}		

		FrameDecoder::processImage(_cropLeft, _cropRight, _cropTop, _cropBottom,
			jpgBuffer.data(), nullptr, _width, _height, _width, (_subsamp == TJSAMP_422) ? PixelFormat::MJPEG : PixelFormat::I420, _lutBuffer, image);
	}
	else if (image.width() != (uint)_width || image.height() != (uint)_height)
	{
		MemoryBuffer<uint8_t> jpgBuffer(_width * _height * 3);

		if (tjDecompress2(_decompress, _localBuffer.data(), _size, jpgBuffer.data(), _width, 0, _height, TJPF_BGR, TJFLAG_BOTTOMUP | TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE) != 0 &&
			tjGetErrorCode(_decompress) == TJERR_FATAL)
			{
				emit SignalNewFrameError(_workerIndex, QString(tjGetErrorStr()), _currentFrame);
				return;
			}					
		
		FrameDecoder::processImage(_cropLeft, _cropRight, _cropTop, _cropBottom,
			jpgBuffer.data(), nullptr, _width, _height, _width * 3, PixelFormat::RGB24, nullptr, image);
	}
	else
	{
		if (tjDecompress2(_decompress, _localBuffer.data(), _size, image.rawMem(), _width, 0, _height, TJPF_RGB, TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE) != 0 &&
			tjGetErrorCode(_decompress) == TJERR_FATAL)
			{
				emit SignalNewFrameError(_workerIndex, QString(tjGetErrorStr()), _currentFrame);
				return;
			}		
	}
	
	image.setBufferCacheSize();
	if (!_directAccess)
		emit SignalNewFrame(_workerIndex, image, _currentFrame, _frameBegin);
	else
	{
		emit GlobalSignals::getInstance()->SignalNewVideoImage(_deviceName, image);
        emit SignalNewFrame(_workerIndex, Image<ColorRgb>(), _currentFrame, _frameBegin);
	}
}
