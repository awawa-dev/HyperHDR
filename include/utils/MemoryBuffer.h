#pragma once

/* MemoryBuffer.h
*
*  MIT License
*
*  Copyright (c) 2020-2023 awawa-dev
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
	#include <utils/MemoryBuffer.h>
	#include <cstdlib>
#endif

template <typename T>
class MemoryBuffer
{
public:
	MemoryBuffer() :
		_buffer(nullptr),
		_size(0)
	{
	}

	MemoryBuffer(size_t size) :
		_buffer( (size > 0) ? reinterpret_cast<T*>(malloc(size)) : nullptr),
		_size(size)
	{
	}

	MemoryBuffer(const MemoryBuffer& t) = delete;
	MemoryBuffer& operator=(const MemoryBuffer& x) = delete;

	~MemoryBuffer()
	{
		releaseMemory();
	}

	void releaseMemory()
	{
		if (_buffer != nullptr)
			free(_buffer);
		_buffer = nullptr;
		_size = 0;
	}

	void resize(size_t size)
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
	}

	inline T* data()
	{
		return _buffer;
	}

	inline size_t size()
	{
		return _size;
	}

private:
	T* _buffer;
	size_t _size;
};

template class MemoryBuffer<uint8_t>;
