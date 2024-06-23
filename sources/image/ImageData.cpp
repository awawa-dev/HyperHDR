/* ImageData.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2024 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/HyperHDR
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
*/

#ifndef PCH_ENABLED
	#include <cmath>
	#include <vector>
	#include <cstdint>
	#include <cstring>
	#include <algorithm>
	#include <cassert>
	#include <type_traits>
#endif

#include <image/ImageData.h>
#include <image/VideoMemoryManager.h>

#define LOCAL_VID_ALIGN_SIZE       16

static_assert (sizeof(ColorRgb) < LOCAL_VID_ALIGN_SIZE && sizeof(ColorRgb) <= sizeof(uint64_t), "Unexpected image size");

template <typename ColorSpace>
ImageData<ColorSpace>::ImageData(unsigned width, unsigned height) :
	_width(width),
	_height(height),
	_memoryBuffer(nullptr),
	_pixels(nullptr)
{
	getMemory(width, height);
}

template <typename ColorSpace>
bool ImageData<ColorSpace>::setBufferCacheSize()
{
	if (_memoryBuffer != nullptr)
		return VideoMemoryManager::videoCache.setFrameSize(_memoryBuffer->size());
	else
		return false;
}

template <typename ColorSpace>
ImageData<ColorSpace>::~ImageData()
{
	freeMemory();
}

template <typename ColorSpace>
unsigned ImageData<ColorSpace>::width() const
{
	return _width;
}

template <typename ColorSpace>
unsigned ImageData<ColorSpace>::height() const
{
	return _height;
}

template <>
const ColorRgb& ImageData<ColorRgb>::operator()(unsigned x, unsigned y) const
{
	return *((ColorRgb*)&(_pixels[toIndex(x, y)]));
}

template <>
ColorRgb& ImageData<ColorRgb>::operator()(unsigned x, unsigned y)
{
	return *((ColorRgb*)&(_pixels[toIndex(x, y)]));
}

template <typename ColorSpace>
void ImageData<ColorSpace>::resize(unsigned width, unsigned height)
{
	if (width == _width && height == _height)
		return;

	if ((width * height) > unsigned((_width * _height)))
	{
		freeMemory();
		getMemory(width, height);
	}

	_width = width;
	_height = height;
}

template <typename ColorSpace>
uint8_t* ImageData<ColorSpace>::rawMem()
{
	return _pixels;
}

template <typename ColorSpace>
const uint8_t* ImageData<ColorSpace>::rawMem() const
{
	return _pixels;
}

template <>
void ImageData<ColorRgb>::toRgb(ImageData<ColorRgb>& image) const
{
	if (image.width() != _width || image.height() != _height)
		image.resize(_width, _height);

	memcpy(image.rawMem(), _pixels, static_cast<size_t>(_width) * _height * 3);
}

template <typename ColorSpace>
size_t ImageData<ColorSpace>::size() const
{
	return  static_cast<size_t>(_width) * static_cast<size_t>(_height) * sizeof(ColorSpace);
}

template <>
void ImageData<ColorRgb>::clear()
{
	if (_pixels != nullptr)
		memset(_pixels, 0, static_cast<size_t>(_width) * _height * 3);
}

template <>
bool ImageData<ColorRgb>::checkSignal(int x, int y, int r, int g, int b, int tolerance)
{
	int index = (y * _width + x) * 3;
	ColorRgb* rgb = ((ColorRgb*)&(_pixels[index]));
	int delta = std::abs(r - rgb->red) + std::abs(g - rgb->green) + std::abs(b - rgb->blue);

	return delta <= tolerance;
}

template <typename ColorSpace>
std::string ImageData<ColorSpace>::adjustCache()
{
	return VideoMemoryManager::videoCache.adjustCache();
}

template <typename ColorSpace>
inline unsigned ImageData<ColorSpace>::toIndex(unsigned x, unsigned y) const
{
	return (y * _width + x) * sizeof(ColorSpace);
}

template <typename ColorSpace>
inline void ImageData<ColorSpace>::getMemory(size_t width, size_t height)
{
	size_t neededSize = width * height * sizeof(ColorSpace) + LOCAL_VID_ALIGN_SIZE;
	_memoryBuffer = VideoMemoryManager::videoCache.request(neededSize);
	_pixels = _memoryBuffer->data();
}

template <typename ColorSpace>
inline void ImageData<ColorSpace>::freeMemory()
{
	VideoMemoryManager::videoCache.release(_memoryBuffer);
	_pixels = nullptr;
}

template <>
void ImageData<ColorRgb>::fastBox(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t r, uint8_t g, uint8_t b)
{
	size_t X1 = std::min(std::min(x1, x2), (uint16_t)(width() - 1));
	size_t X2 = std::min(std::max(x1, x2), (uint16_t)(width() - 1));
	size_t Y1 = std::min(std::min(y1, y2), (uint16_t)(height() - 1));
	size_t Y2 = std::min(std::max(y1, y2), (uint16_t)(height() - 1));

	size_t patternSize = (X2 - X1 + 1) * 3;
	size_t lsize = width() * 3;

	uint8_t* pattern = rawMem() + Y1 * lsize + X1 * 3;
	for (size_t i = 0; i < patternSize; i += 3)
	{
		pattern[i] = r;
		pattern[i + 1] = g;
		pattern[i + 2] = b;
	}


	for (size_t y = Y1 + 1; y <= Y2; y++)
		memcpy(rawMem() + y * lsize + X1 * 3, pattern, patternSize);
}

template <>
void ImageData<ColorRgb>::gradientHBox(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t r, uint8_t g, uint8_t b)
{
	int X1 = std::min(std::min(x1, x2), (uint16_t)(width() - 1));
	int X2 = std::min(std::max(x1, x2), (uint16_t)(width() - 1));
	int Y1 = std::min(std::min(y1, y2), (uint16_t)(height() - 1));
	int Y2 = std::min(std::max(y1, y2), (uint16_t)(height() - 1));

	if (X2 - X1 < 4)
	{
		fastBox(X1, Y1, X2, Y2, r, g, b);
		return;
	}

	int Xmin = X1 - 4;
	int Xmax = X2 + 4;

	uint16_t rr = r / 6, gg = g / 6, bb = b / 6;

	for (int X = Xmin; X <= X1 + 1; X++)
	{
		if (X >= 0)
			fastBox(X, y1, X, y2, rr, gg, bb);
		rr = std::min(rr + r / 6, 255);
		gg = std::min(gg + g / 6, 255);
		bb = std::min(bb + b / 6, 255);
	}

	fastBox(X1 + 2, Y1, X2 - 2, Y2, r, g, b);

	for (int X = X2 - 1; X < Xmax; X++)
	{
		if (X < (int)width())
			fastBox(X, y1, X, y2, rr, gg, bb);
		rr = std::max(rr - r / 6, 0);
		gg = std::max(gg - g / 6, 0);
		bb = std::max(bb - b / 6, 0);
	}
}

template <>
void ImageData<ColorRgb>::gradientVBox(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t r, uint8_t g, uint8_t b)
{
	int X1 = std::min(std::min(x1, x2), (uint16_t)(width() - 1));
	int X2 = std::min(std::max(x1, x2), (uint16_t)(width() - 1));
	int Y1 = std::min(std::min(y1, y2), (uint16_t)(height() - 1));
	int Y2 = std::min(std::max(y1, y2), (uint16_t)(height() - 1));

	if (Y2 - Y1 < 4)
	{
		fastBox(X1, Y1, X2, Y2, r, g, b);
		return;
	}

	int Ymin = Y1 - 4;
	int Ymax = Y2 + 4;

	uint16_t rr = r / 6, gg = g / 6, bb = b / 6;

	for (int Y = Ymin; Y <= Y1 + 1; Y++)
	{
		if (Y >= 0)
			fastBox(x1, Y, x2, Y, rr, gg, bb);
		rr = std::min(rr + r / 6, 255);
		gg = std::min(gg + g / 6, 255);
		bb = std::min(bb + b / 6, 255);
	}

	fastBox(x1, Y1 + 2, x2, Y2 - 2, r, g, b);

	for (int Y = Y2 - 1; Y < Ymax; Y++)
	{
		if (Y < (int)height())
			fastBox(x1, Y, x2, Y, rr, gg, bb);
		rr = std::max(rr - r / 6, 0);
		gg = std::max(gg - g / 6, 0);
		bb = std::max(bb - b / 6, 0);
	}
}

template class ImageData<ColorRgb>;
