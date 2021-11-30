#pragma once
#include <cstdint>
#include <stdlib.h>

#include <QString>
#include <QStack>
#include <QMutex>
#include <QMutexLocker>

#define VideoMemoryManagerBufferSize 8

class VideoMemoryManager
{
public:
	VideoMemoryManager(int bufferSize = VideoMemoryManagerBufferSize);
	~VideoMemoryManager();

	bool	setFrameSize(size_t size);	
	void    release(size_t size, uint8_t* buffer);
	QString	adjustCache();
	uint8_t*	request(size_t size);

	static void enableCache(bool frameCache);

private:
	void releaseBuffer();

	QStack<uint8_t*> _stack;
	size_t           _currentSize;
	int              _prevSize;
	int              _hits;
	bool             _needed;
	bool             _prevNeeded;
	int              _bufferLimit;
	QMutex           _locker;

	static bool      _enabled, _dirty;
};
