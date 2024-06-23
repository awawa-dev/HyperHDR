#pragma once

#ifndef PCH_ENABLED
	#include <mutex>
	#include <list>
	#include <cstdint>
	#include <cstring>
	#include <sstream>
	#include <stdlib.h>
	#include <memory>
#endif

#include <image/MemoryBuffer.h>

#define VideoMemoryManagerBufferSize 8

class VideoMemoryManager
{
public:
	VideoMemoryManager(int bufferSize = VideoMemoryManagerBufferSize);
	~VideoMemoryManager();

	bool	setFrameSize(size_t size);
	void    release(std::unique_ptr<MemoryBuffer<uint8_t>>& frame);
	std::string	adjustCache();
	std::unique_ptr<MemoryBuffer<uint8_t>> request(size_t size);

	static VideoMemoryManager videoCache;

private:
	void releaseBuffer();

	std::list<std::unique_ptr<MemoryBuffer<uint8_t>>> _stack;
	size_t           _currentSize;
	size_t           _prevSize;
	int              _hits;
	bool             _needed;
	bool             _prevNeeded;
	size_t           _bufferLimit;
	std::mutex       _locker;
};
