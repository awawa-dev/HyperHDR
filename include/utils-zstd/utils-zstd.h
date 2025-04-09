#pragma once

#ifndef _ZSTD_SHARED_API
	#define _ZSTD_SHARED_API
#endif

#include <image/MemoryBuffer.h>

_ZSTD_SHARED_API const char* DecompressZSTD(size_t downloadedDataSize, const uint8_t* downloadedData, const char* fileName);
_ZSTD_SHARED_API const char* DecompressZSTD(size_t downloadedDataSize, const uint8_t* downloadedData, uint8_t* dest, int destSeek, int destSize);
