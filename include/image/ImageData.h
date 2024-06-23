#pragma once

#ifndef PCH_ENABLED
	#include <memory>
#endif

#include <image/MemoryBuffer.h>
#include <image/ColorRgb.h>

template <typename ColorSpace>
class ImageData
{
public:
	ImageData(unsigned width, unsigned height);

	bool setBufferCacheSize();

	~ImageData();

	unsigned width() const;

	unsigned height() const;

	const ColorSpace& operator()(unsigned x, unsigned y) const;

	ColorSpace& operator()(unsigned x, unsigned y);

	void resize(unsigned width, unsigned height);

	uint8_t* rawMem();

	const uint8_t* rawMem() const;

	void toRgb(ImageData<ColorSpace>& image) const;

	size_t size() const;

	void clear();

	bool checkSignal(int x, int y, int r, int g, int b, int tolerance);

	void fastBox(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t r, uint8_t g, uint8_t b);

	void gradientHBox(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t r, uint8_t g, uint8_t b);

	void gradientVBox(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t r, uint8_t g, uint8_t b);

	static std::string adjustCache();

private:
	inline unsigned toIndex(unsigned x, unsigned y) const;

	inline void getMemory(size_t width, size_t height);

	inline void freeMemory();

private:
	unsigned _width;
	unsigned _height;

	std::unique_ptr<MemoryBuffer<uint8_t>> _memoryBuffer;

	uint8_t* _pixels;
};
