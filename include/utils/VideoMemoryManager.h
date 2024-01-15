#pragma once

#ifndef PCH_ENABLED
	#include <QString>
	#include <QStack>
	#include <QMutex>
	#include <QMutexLocker>

	#include <list>
	#include <cstdint>
	#include <stdlib.h>

	#include <utils/MemoryBuffer.h>
#endif

#define VideoMemoryManagerBufferSize 8

class VideoMemoryManager
{
public:
	VideoMemoryManager(int bufferSize = VideoMemoryManagerBufferSize);
	~VideoMemoryManager();

	bool	setFrameSize(size_t size);
	void    release(std::unique_ptr<MemoryBuffer<uint8_t>>& frame);
	QString	adjustCache();
	std::unique_ptr<MemoryBuffer<uint8_t>> request(size_t size);

private:
	void releaseBuffer();

	std::list<std::unique_ptr<MemoryBuffer<uint8_t>>> _stack;
	size_t           _currentSize;
	size_t           _prevSize;
	int              _hits;
	bool             _needed;
	bool             _prevNeeded;
	size_t           _bufferLimit;
	QMutex           _locker;
};
