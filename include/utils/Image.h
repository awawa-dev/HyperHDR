#pragma once

#include <QExplicitlySharedDataPointer>
#include <utils/ImageData.h>
#include <cmath>

template <typename Pixel_T>
class Image
{
public:
	typedef Pixel_T pixel_type;

	Image() :
		Image(1, 1)
	{
	}

	Image(unsigned width, unsigned height) :
		_d_ptr(new ImageData(width, height))
	{
	}


	///
	/// Copy constructor for an image
	/// @param other The image which will be copied
	///
	Image(const Image& other)
	{
		_d_ptr = other._d_ptr;
	}

	Image& operator=(Image rhs)
	{
		_d_ptr = rhs._d_ptr;
		return *this;
	}

	static QString adjustCache()
	{
		return ImageData::adjustCache();
	}

	bool setBufferCacheSize() const
	{
		return _d_ptr->setBufferCacheSize();
	}

	bool checkSignal(int x, int y, int r, int g, int b, int tolerance)
	{
		return _d_ptr->checkSignal(x, y, r, g, b, tolerance);
	}

	void fastBox(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t r, uint8_t g, uint8_t b)
	{
		size_t X1 = std::min(std::min(x1, x2), (uint16_t)(width() - 1));
		size_t X2 = std::min(std::max(x1, x2), (uint16_t)(width() - 1));
		size_t Y1 = std::min(std::min(y1, y2), (uint16_t)(height() - 1));
		size_t Y2 = std::min(std::max(y1, y2), (uint16_t)(height() - 1));

		size_t patternSize = (X2 - X1 + 1) * 3;
		size_t lsize = width() * 3;

		uint8_t* pattern = ((uint8_t*)memptr()) + Y1 * lsize + X1 * 3;
		for (size_t i = 0; i < patternSize; i += 3)
		{
			pattern[i] = r;
			pattern[i + 1] = g;
			pattern[i + 2] = b;
		}


		for (size_t y = Y1 + 1; y <= Y2; y++)
			memcpy(((uint8_t*)memptr()) + y * lsize + X1 * 3, pattern, patternSize);
	}

	void gradientHBox(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t r, uint8_t g, uint8_t b)
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

	void gradientVBox(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t r, uint8_t g, uint8_t b)
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

	///
	/// Returns the width of the image
	///
	/// @return The width of the image
	///
	inline unsigned width() const
	{
		return _d_ptr->width();
	}

	///
	/// Returns the height of the image
	///
	/// @return The height of the image
	///
	inline unsigned height() const
	{
		return _d_ptr->height();
	}

	/// Returns a reference to a specified pixel in the image
	///
	/// @param x The x index
	/// @param y The y index
	const Pixel_T& operator()(unsigned x, unsigned y) const
	{
		return _d_ptr->operator()(x, y);
	}

	///
	/// @return reference to specified pixel
	///
	Pixel_T& operator()(unsigned x, unsigned y)
	{
		return _d_ptr->operator()(x, y);
	}

	/// Resize the image
	/// @param width The width of the image
	/// @param height The height of the image
	void resize(unsigned width, unsigned height)
	{
		_d_ptr->resize(width, height);
	}

	///
	/// Returns a memory pointer to the first pixel in the image
	/// @return The memory pointer to the first pixel
	///
	Pixel_T* memptr()
	{
		return _d_ptr->memptr();
	}

	///
	/// Returns a const memory pointer to the first pixel in the image
	/// @return The const memory pointer to the first pixel
	///
	const Pixel_T* memptr() const
	{
		return _d_ptr->memptr();
	}

	///
	/// Get size of buffer
	///
	size_t size() const
	{
		return _d_ptr->size();
	}

	///
	/// Clear the image
	///
	void clear()
	{
		_d_ptr->clear();
	}

private:
	QExplicitlySharedDataPointer<ImageData>  _d_ptr;
};

