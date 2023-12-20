/* VideoMemoryManager.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2023 awawa-dev
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

#ifndef PCH_ENABLED
	#include <utils/VideoMemoryManager.h>
#endif

#define BUFFER_LOWER_LIMIT 1

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
	if (_currentSize != size)
	{
		QMutexLocker lockme(&_locker);

		releaseBuffer();

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

std::unique_ptr<MemoryBuffer<uint8_t>> VideoMemoryManager::request(size_t size)
{
	if (size != _currentSize)
	{
		return std::unique_ptr<MemoryBuffer<uint8_t>>(new MemoryBuffer<uint8_t>(size));
	}

	QMutexLocker lockme(&_locker);

	if (_stack.size() == 0)
	{
		return std::unique_ptr<MemoryBuffer<uint8_t>>(new MemoryBuffer<uint8_t>(size));
	}
	else
	{
		_hits++;

		std::unique_ptr<MemoryBuffer<uint8_t>> retVal = std::move(_stack.back());
		_stack.pop_back();

		if (_stack.size() <= BUFFER_LOWER_LIMIT)
			_needed = true;

		return retVal;
	}
}

void VideoMemoryManager::release(std::unique_ptr<MemoryBuffer<uint8_t>>& frame)
{
	if (frame->size() != _currentSize)
	{
		return;
	}

	QMutexLocker lockme(&_locker);

	if (_stack.size() >= _bufferLimit)
	{
		return;
	}

	_stack.push_back(std::move(frame));
}

void VideoMemoryManager::releaseBuffer()
{
	_stack.clear();
}

QString VideoMemoryManager::adjustCache()
{
	size_t curSize = 0;
	int hits = 0;
	bool needed = false, cleanup = false;
	bool info = false;

	QMutexLocker lockme(&_locker);

	if (_prevNeeded != _needed || _prevSize != _stack.size())
		info = true;

	// clean up buffer if neccesery
	if (!_needed && !_prevNeeded && _stack.size() > 0)
	{
		_stack.pop_back();
		cleanup = true;
		_prevNeeded = true;
	}
	else
		_prevNeeded = _needed;

	_prevSize = _stack.size();

	// get stats
	curSize = _stack.size();

	hits = _hits;
	needed = _needed;

	// clear stats
	_hits = 0;
	_needed = false;

	if (info || cleanup)
		return QString("Video cache: size: %1, hits: %2, needed: %3, cleanup: %4, limit: %5").
						arg(curSize).arg(hits).arg(needed).arg(cleanup).arg(_bufferLimit);
	else
		return "";
}

