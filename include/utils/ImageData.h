#pragma once

// Includes
#include <utils/ColorRgb.h>
#include <utils/VideoMemoryManager.h>

// QT includes
#include <QSharedData>

#if defined(_MSC_VER)
	#include <BaseTsd.h>
	typedef SSIZE_T ssize_t;
#endif


template <typename ColorSpace>
class ImageData : public QSharedData
{	
public:
	ImageData(unsigned width, unsigned height);

	ImageData(const ImageData<ColorSpace>& other);

	bool setBufferCacheSize();
	
	~ImageData<ColorSpace>();

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

	static QString adjustCache();

private:
	inline unsigned toIndex(unsigned x, unsigned y) const;

	inline uint8_t* getMemory(size_t width, size_t height);

	inline void freeMemory();

private:
	/// The width of the image
	unsigned _width;
	/// The height of the image
	unsigned _height;

	uint64_t _initData;

	/// The pixels of the image
	uint8_t* _pixels;

	size_t   _bufferSize;
	
	static VideoMemoryManager videoCache;
};
