#pragma once
#include <cstdint>
#include <stdlib.h>

#include <QString>
#include <QStack>
#include <QSemaphore>

#define VideoMemoryManagerBufferSize 10

class VideoMemoryManager
{
public:
	VideoMemoryManager(int bufferSize = VideoMemoryManagerBufferSize);
	~VideoMemoryManager();

	bool     SetFrameSize(size_t size);
	uint8_t* Request(size_t size);
	void     Release(size_t size, uint8_t* buffer);
	QString  GetInfo();

	static void EnableCache(bool frameCache);
private :
	void     ReleaseBuffer();

	QStack<uint8_t*> _stack;
	size_t           _currentSize;
	int              _bufferLimit;
	QSemaphore       _synchro;

	static bool      _enabled, _dirty;
};
