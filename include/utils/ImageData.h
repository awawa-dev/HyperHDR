#pragma once

// STL includes
#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cassert>
#include <type_traits>
#include <utils/ColorRgb.h>
#include <utils/VideoMemoryManager.h>

// QT includes
#include <QSharedData>

// https://docs.microsoft.com/en-us/windows/win32/winprog/windows-data-types#ssize-t
#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif



class ImageData : public QSharedData
{	
	template<class T>
	friend class Image;

public:
	ImageData(unsigned width, unsigned height):
		_width(width),
		_height(height),
		_pixels(getMemory(width, height))
	{		
	}

	ImageData(const ImageData & other) :		
		_width(other._width),
		_height(other._height),
		_pixels(getMemory(other._width, other._height))
	{
		if (_pixels != NULL)
			memcpy(_pixels, other._pixels, static_cast<size_t>(other._width) * other._height * 3);
	}

	bool setBufferCacheSize()
	{
		return videoCache.SetFrameSize(_bufferSize);
	}


	ImageData& operator=(ImageData rhs)
	{
		rhs.swap(*this);
		return *this;
	}

	void swap(ImageData& s) noexcept
	{
		using std::swap;
		swap(this->_width, s._width);
		swap(this->_height, s._height);
		swap(this->_pixels, s._pixels);
		swap(this->_bufferSize, s._bufferSize);
	}

	ImageData(ImageData&& src) noexcept
		: _width(0)
		, _height(0)
		, _pixels(NULL)
	{
		src.swap(*this);
	}

	ImageData& operator=(ImageData&& src) noexcept
	{
		src.swap(*this);
		return *this;
	}

	~ImageData()
	{
		freeMemory();
	}

	inline unsigned width() const
	{
		return _width;
	}

	inline unsigned height() const
	{
		return _height;
	}	

	const ColorRgb& operator()(unsigned x, unsigned y) const
	{
		return *((ColorRgb*)&(_pixels[toIndex(x,y)]));
	}

	ColorRgb& operator()(unsigned x, unsigned y)
	{
		return *((ColorRgb*)&(_pixels[toIndex(x,y)]));
	}

	void resize(unsigned width, unsigned height)
	{
		if (width == _width && height == _height)
			return;

		if ((width * height) > unsigned((_width * _height)))
		{
			freeMemory();
			_pixels = (uint8_t*) getMemory(width, height);
		}

		_width = width;
		_height = height;
	}

	ColorRgb* memptr()
	{
		return (ColorRgb*) _pixels;
	}

	const ColorRgb* memptr() const
	{
		return (ColorRgb*) _pixels;
	}

	void toRgb(ImageData& image) const
	{
		if (image.width() != _width || image.height() != _height)
			image.resize(_width, _height);
		
		memcpy(image.memptr(), _pixels, static_cast<size_t>(_width) * _height *3 );
	}

	size_t size() const
	{
		return  static_cast<size_t>(_width) * static_cast<size_t>(_height) * 3;
	}

	void clear()
	{
		if (_width != 1 || _height != 1)
		{
			_width = 1;
			_height = 1;
			freeMemory();
			_pixels = getMemory(_width, _height);
		}
		if (_pixels != NULL)
			memset(_pixels, 0, static_cast<size_t>(_width) * _height * 3);
	}

	bool checkSignal(int x, int y, int r, int g, int b, int tolerance)
	{
		int index = (y * _width + x) * 3;
		ColorRgb*  rgb = ((ColorRgb*)&(_pixels[index]));
		int delta = std::abs(r - rgb->red) + std::abs(g - rgb->green) + std::abs(b - rgb->blue);

		return delta <= tolerance;
	}

	QString getCacheInfo()
	{		
		return videoCache.GetInfo();
	}

private:
	inline unsigned toIndex(unsigned x, unsigned y) const
	{
		return (y * _width  + x) * 3;
	}

	inline uint8_t* getMemory(size_t width, size_t height)
	{
		if (width == 1 && height == 1)
		{
			_bufferSize = 3;
			return initDataPointer;
		}

		_bufferSize = width * height * 3 + 16;
		return videoCache.Request(_bufferSize);
	}

	inline void freeMemory()
	{
		if (_pixels == initDataPointer)
			return;

		if (_pixels != nullptr)
		{
			videoCache.Release(_bufferSize, _pixels);
			_bufferSize = 0;
			_pixels = nullptr;
		}
	}

private:
	/// The width of the image
	unsigned _width;
	/// The height of the image
	unsigned _height;
	/// The pixels of the image
	uint8_t* _pixels;

	size_t   _bufferSize;

	static uint64_t           initData;
	static uint8_t*     initDataPointer;
	static VideoMemoryManager videoCache;
};
