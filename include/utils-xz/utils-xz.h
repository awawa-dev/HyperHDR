#pragma once

#ifndef _XZ_SHARED_API
	#define _XZ_SHARED_API
#endif

_XZ_SHARED_API const char* DecompressXZ(size_t downloadedDataSize, const uint8_t* downloadedData, const char* fileName);
