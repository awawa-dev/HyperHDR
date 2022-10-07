#pragma once

#include <QExplicitlySharedDataPointer>
#include <utils/ImageData.h>

template <typename ColorSpace>
class Image
{
public:
	Image();

	Image(unsigned width, unsigned height);

	///
	/// Copy constructor for an image
	/// @param other The image which will be copied
	///
	Image(const Image<ColorSpace>& other);

	Image<ColorSpace>& operator=(Image<ColorSpace> other);

	static QString adjustCache();

	bool setBufferCacheSize() const;

	bool checkSignal(int x, int y, int r, int g, int b, int tolerance);

	void fastBox(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t r, uint8_t g, uint8_t b);

	void gradientHBox(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t r, uint8_t g, uint8_t b);

	void gradientVBox(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t r, uint8_t g, uint8_t b);

	///
	/// Returns the width of the image
	///
	/// @return The width of the image
	///
	unsigned width() const;

	///
	/// Returns the height of the image
	///
	/// @return The height of the image
	///
	unsigned height() const;

	/// Returns a reference to a specified pixel in the image
	///
	/// @param x The x index
	/// @param y The y index
	const ColorSpace& operator()(unsigned x, unsigned y) const;

	///
	/// @return reference to specified pixel
	///
	ColorSpace& operator()(unsigned x, unsigned y);

	/// Resize the image
	/// @param width The width of the image
	/// @param height The height of the image
	void resize(unsigned width, unsigned height);

	///
	/// Returns a memory pointer to the first pixel in the image
	/// @return The memory pointer to the first pixel
	///
	uint8_t* rawMem();

	///
	/// Returns a const memory pointer to the first pixel in the image
	/// @return The const memory pointer to the first pixel
	///
	const uint8_t* rawMem() const;

	///
	/// Get size of buffer
	///
	size_t size() const;

	///
	/// Clear the image
	///
	void clear();

private:
	QExplicitlySharedDataPointer<ImageData<ColorSpace>>  _d_ptr;
};

Q_DECLARE_TYPEINFO(Image<ColorRgb>, Q_MOVABLE_TYPE);

