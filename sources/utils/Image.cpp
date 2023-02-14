/* Image.cpp
*
*  MIT License
*
*  Copyright (c) 2022 awawa-dev
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

#include <utils/Image.h>

template <typename ColorSpace>
Image<ColorSpace>::Image() :
		Image(1, 1)
{
}

template <typename ColorSpace>
Image<ColorSpace>::Image(unsigned width, unsigned height) :
	_d_ptr(new ImageData<ColorSpace>(width, height))
{
}

template <typename ColorSpace>
Image<ColorSpace>::Image(const Image<ColorSpace>& other):
	_d_ptr(other._d_ptr)
{	
}

template <typename ColorSpace>
Image<ColorSpace>& Image<ColorSpace>::operator=(Image<ColorSpace> other)
{
	_d_ptr = other._d_ptr;
	return *this;
}

template <typename ColorSpace>
QString Image<ColorSpace>::adjustCache()
{
	return ImageData<ColorSpace>::adjustCache();
}

template <typename ColorSpace>
bool Image<ColorSpace>::setBufferCacheSize() const
{
	return _d_ptr->setBufferCacheSize();
}

template <typename ColorSpace>
bool Image<ColorSpace>::checkSignal(int x, int y, int r, int g, int b, int tolerance)
{
	return _d_ptr->checkSignal(x, y, r, g, b, tolerance);
}

template <typename ColorSpace>
void Image<ColorSpace>::fastBox(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t r, uint8_t g, uint8_t b)
{
	_d_ptr->fastBox(x1, y1, x2, y2, r, g, b);
}

template <typename ColorSpace>
void Image<ColorSpace>::gradientHBox(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t r, uint8_t g, uint8_t b)
{
	_d_ptr->gradientHBox(x1, y1, x2, y2, r, g, b);
}

template <typename ColorSpace>
void Image<ColorSpace>::gradientVBox(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t r, uint8_t g, uint8_t b)
{
	_d_ptr->gradientVBox(x1, y1, x2, y2, r, g, b);
}

template <typename ColorSpace>
unsigned Image<ColorSpace>::width() const
{
	return _d_ptr->width();
}

template <typename ColorSpace>
unsigned Image<ColorSpace>::height() const
{
	return _d_ptr->height();
}

template <typename ColorSpace>
const ColorSpace& Image<ColorSpace>::operator()(unsigned x, unsigned y) const
{
	return _d_ptr->operator()(x, y);
}

template <typename ColorSpace>
ColorSpace& Image<ColorSpace>::operator()(unsigned x, unsigned y)
{
	return _d_ptr->operator()(x, y);
}

template <typename ColorSpace>
void Image<ColorSpace>::resize(unsigned width, unsigned height)
{
	_d_ptr->resize(width, height);
}

template <typename ColorSpace>
uint8_t* Image<ColorSpace>::rawMem()
{
	return _d_ptr->rawMem();
}

template <typename ColorSpace>
const uint8_t* Image<ColorSpace>::rawMem() const
{
	return _d_ptr->rawMem();
}

template <typename ColorSpace>
size_t Image<ColorSpace>::size() const
{
	return _d_ptr->size();
}

template <typename ColorSpace>
void Image<ColorSpace>::clear()
{
	_d_ptr->clear();
}

template class Image<ColorRgb>;
