#include <utils/ImageData.h>

VideoMemoryManager ImageData::videoCache;

uint64_t ImageData::initData = 0;
uint8_t* ImageData::initDataPointer = reinterpret_cast<uint8_t*>(&initData);
