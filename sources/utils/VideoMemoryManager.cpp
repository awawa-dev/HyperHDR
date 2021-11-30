/* VideoMemoryManager.cpp
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

#include <utils/VideoMemoryManager.h>

#define BUFFER_LOWER_LIMIT 1

bool VideoMemoryManager::_enabled(false);
bool VideoMemoryManager::_dirty(false);

VideoMemoryManager::VideoMemoryManager(int bufferSize) :
	_currentSize(0),
	_prevSize(0),
	_hits(0),
	_needed(false),
	_prevNeeded(true),
	_bufferLimit(bufferSize)
{

};

VideoMemoryManager::~VideoMemoryManager()
{
	QMutexLocker lockme(&_locker);

	releaseBuffer();
}

bool VideoMemoryManager::setFrameSize(size_t size)
{
	if (_currentSize != size || (!_enabled && _dirty))
	{
		QMutexLocker lockme(&_locker);

		releaseBuffer();

		_dirty = false;

		_hits = 0;

		_needed = false;

		_prevNeeded = true;

		_bufferLimit = VideoMemoryManagerBufferSize;

		_currentSize = size;

		_prevSize = 0;

		return true;
	}

	return false;
}

uint8_t* VideoMemoryManager::request(size_t size)
{
	uint8_t* retVal;

	if (size != _currentSize || !_enabled)
	{
		return static_cast<uint8_t*>(malloc(size));
	}

	QMutexLocker lockme(&_locker);

	if (_stack.isEmpty())
	{
		retVal = static_cast<uint8_t*>(malloc(size));
	}
	else
	{
		_hits++;

		retVal = _stack.pop();

		if (_stack.length() <= BUFFER_LOWER_LIMIT)
			_needed = true;
	}

	return retVal;
}

void VideoMemoryManager::release(size_t size, uint8_t* buffer)
{
	if (size != _currentSize || !_enabled)
	{
		free(buffer);
		return;
	}

	QMutexLocker lockme(&_locker);

	if (_stack.length() >= _bufferLimit)
	{
		free(buffer);
		return;
	}

	_stack.push(buffer);
}

void VideoMemoryManager::releaseBuffer()
{
	while (!_stack.isEmpty())
	{
		auto pointer = _stack.pop();
		free(pointer);
	}
}

void VideoMemoryManager::enableCache(bool frameCache)
{
	if (_enabled == frameCache)
		return;

	_enabled = frameCache;

	_dirty = true;
}

QString VideoMemoryManager::adjustCache()
{
	int curSize = -1, hits = -1;
	bool needed = false, cleanup = false;
	bool info = false;

	QMutexLocker lockme(&_locker);

	if (_prevNeeded != _needed || _prevSize != _stack.length())
		info = true;

	// clean up buffer if neccesery
	if (!_needed && !_prevNeeded && _stack.length() > 0)
	{
		auto pointer = _stack.pop();
		free(pointer);
		cleanup = true;
		_prevNeeded = true;
	}
	else
		_prevNeeded = _needed;

	_prevSize = _stack.length();

	// get stats
	curSize = _stack.length();

	hits = _hits;
	needed = _needed;

	// clear stats
	_hits = 0;
	_needed = false;

	if (info || cleanup)
		return QString("Video cache: %1, size: %2, hits: %3, needed: %4, cleanup: %5, limit: %6").
						arg((_enabled) ? "enabled" : "disabled").arg(curSize).arg(hits).arg(needed).arg(cleanup).arg(_bufferLimit);
	else
		return "";
}

