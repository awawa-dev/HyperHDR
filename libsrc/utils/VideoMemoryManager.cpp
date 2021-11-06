#include <utils/VideoMemoryManager.h>

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
	if (_currentSize != size)
	{
		_synchro.acquire(1);

			ReleaseBuffer();

			_currentSize = size;

		_synchro.release(1);

		return true;
	}

	return false;
}

uint8_t* VideoMemoryManager::Request(size_t size)
{
	uint8_t* retVal;

	if (size != _currentSize)
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
	if (size != _currentSize)
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
