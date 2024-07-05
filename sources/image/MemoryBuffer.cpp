/* MemoryBuffer.cpp
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

#include <image/MemoryBuffer.h>
#include <cstdint>

template <typename T>
MemoryBuffer<T>::MemoryBuffer() :
	_buffer(nullptr),
	_size(0)
{
};

template <typename T>
MemoryBuffer<T>::MemoryBuffer(size_t size) :
	_buffer((size > 0) ? reinterpret_cast<T*>(malloc(size)) : nullptr),
	_size(size)
{
};



template <typename T>
MemoryBuffer<T>::~MemoryBuffer()
{
	releaseMemory();
};

template <typename T>
void MemoryBuffer<T>::releaseMemory()
{
	if (_buffer != nullptr)
		free(_buffer);
	_buffer = nullptr;
	_size = 0;
};

template <typename T>
void MemoryBuffer<T>::resize(size_t size)
{
	if (size == 0)
	{
		releaseMemory();
	}
	else if (_size != size)
	{
		auto newBuffer = reinterpret_cast<T*>(realloc(_buffer, size));
		_buffer = newBuffer;
		_size = size;
	}
};

template <typename T>
T* MemoryBuffer<T>::data() const
{
	return _buffer;
};

template <typename T>
size_t MemoryBuffer<T>::size() const
{
	return _size;
};

template class MemoryBuffer<uint8_t>;
