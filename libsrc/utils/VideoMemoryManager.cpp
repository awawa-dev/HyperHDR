#include <utils/VideoMemoryManager.h>

#define BUFFER_LOWER_LIMIT 1

bool VideoMemoryManager::_enabled(false);
bool VideoMemoryManager::_dirty(false);

VideoMemoryManager::VideoMemoryManager(int bufferSize):	
	_currentSize(0),
	_hits(0),
	_needed(false),
	_prevNeeded(true),
	_bufferLimit(bufferSize),
	_synchro(1)
{

};

VideoMemoryManager::~VideoMemoryManager()
{
	_synchro.acquire(1);

		ReleaseBuffer();

	_synchro.release(1);
}

bool VideoMemoryManager::SetFrameSize(size_t size)
{
	if (_currentSize != size || (!_enabled && _dirty))
	{
		_synchro.acquire(1);

			ReleaseBuffer();

			_dirty = false;

			_hits = 0;

			_needed = false;

			_prevNeeded = true;

			_bufferLimit = VideoMemoryManagerBufferSize;

			_currentSize = size;

		_synchro.release(1);

		return true;
	}

	return false;
}

uint8_t* VideoMemoryManager::Request(size_t size)
{
	uint8_t* retVal;

	if (size != _currentSize || !_enabled)
	{
		return static_cast<uint8_t*>(malloc(size));
	}

	_synchro.acquire(1);

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

	_synchro.release(1);

	return retVal;
}

void VideoMemoryManager::Release(size_t size, uint8_t* buffer)
{
	if (size != _currentSize || !_enabled)
	{
		free(buffer);
		return;
	}

	_synchro.acquire(1);

		if (_stack.length() >= _bufferLimit)
		{
			free(buffer);
			_synchro.release(1);
			return;
		}

		_stack.push(buffer);

	_synchro.release(1);
}

void VideoMemoryManager::ReleaseBuffer()
{
	while (!_stack.isEmpty())
	{
		auto pointer = _stack.pop();
		free(pointer);
	}
}

void VideoMemoryManager::EnableCache(bool frameCache)
{
	if (_enabled == frameCache)
		return;

	_enabled = frameCache;

	_dirty = true;
}

QString VideoMemoryManager::GetInfo()
{
	int curSize = -1, hits = -1;
	bool needed = false, cleanup = false;

	_synchro.acquire(1);

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

	// get stats
	curSize = _stack.length();

	hits = _hits;	
	needed = _needed;

	// clear stats
	_hits = 0;	
	_needed = false;

	_synchro.release(1);

	return QString("Video cache: %1, size: %2, hits: %3, needed: %4, cleanup: %5, limit: %6").
		          arg((_enabled)?"enabled":"disabled").arg(curSize).arg(hits).arg(needed).arg(cleanup).arg(_bufferLimit);
}

