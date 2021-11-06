#include <utils/VideoMemoryManager.h>

bool VideoMemoryManager::_enabled(false);
bool VideoMemoryManager::_dirty(false);

VideoMemoryManager::VideoMemoryManager(int bufferSize):	
	_currentSize(0),
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
		retVal = _stack.pop();;
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
	int _size = -1;

	_synchro.acquire(1);

	_size = _stack.length();

	_synchro.release(1);

	return QString("Cache enabled: %1, size: %2, limit: %3").arg(_enabled).arg(_size).arg(_bufferLimit);
}

