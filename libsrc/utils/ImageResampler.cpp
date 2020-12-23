#include "utils/ImageResampler.h"
#include <utils/ColorSys.h>
#include <utils/Logger.h>

ImageResampler::ImageResampler()
	: _horizontalDecimation(1)
	, _verticalDecimation(1)
	, _cropLeft(0)
	, _cropRight(0)
	, _cropTop(0)
	, _cropBottom(0)
{
}

ImageResampler::~ImageResampler()
{
}

void ImageResampler::setHorizontalPixelDecimation(int decimator)
{
	_horizontalDecimation = decimator;
}

void ImageResampler::setVerticalPixelDecimation(int decimator)
{
	_verticalDecimation = decimator;
}

void ImageResampler::setCropping(int cropLeft, int cropRight, int cropTop, int cropBottom)
{
	_cropLeft   = cropLeft;
	_cropRight  = cropRight;
	_cropTop    = cropTop;
	_cropBottom = cropBottom;
}

void ImageResampler::processImage(
	int _cropLeft, int _cropRight, int _cropTop, int _cropBottom,
	const uint8_t* data, int width, int height, int lineLength, const PixelFormat pixelFormat, const uint8_t* lutBuffer, Image<ColorRgb>& outputImage)
{
	uint32_t ind_lutd, ind_lutd2;
	uint8_t  buffer[8];

	// validate format
	if (pixelFormat != PixelFormat::YUYV &&
		pixelFormat != PixelFormat::XRGB && pixelFormat != PixelFormat::RGB24 &&
		pixelFormat != PixelFormat::I420 && pixelFormat != PixelFormat::NV12)
	{
		Error(Logger::getInstance("ImageResampler"), "Invalid pixel format given");
		return;
	}

	// validate format LUT
	if ((pixelFormat == PixelFormat::YUYV || pixelFormat == PixelFormat::I420 ||
		 pixelFormat == PixelFormat::NV12) && lutBuffer == NULL)
	{
		Error(Logger::getInstance("ImageResampler"), "Missing LUT table for YUV colorspace");
		return;
	}

	// sanity check, odd values doesnt work for yuv either way
	_cropLeft = (_cropLeft >> 1) << 1;
	_cropRight = (_cropRight >> 1) << 1;

	// calculate the output size
	int outputWidth = (width - _cropLeft - _cropRight);
	int outputHeight = (height - _cropTop - _cropBottom);

	//if (outputImage.width() != unsigned(outputWidth) || outputImage.height() != unsigned(outputHeight))
	outputImage.resize(outputWidth, outputHeight);

	uint8_t*    destMemory = (uint8_t*) outputImage.memptr();
	int 		destLineSize = outputImage.width() * 3;


	if (pixelFormat == PixelFormat::YUYV)
	{
		for (int yDest = 0, ySource = _cropTop; yDest < outputHeight; ++ySource, ++yDest)
		{
			uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
			uint8_t* endDest = currentDest + destLineSize;
			uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource) + (((uint64_t)_cropLeft) << 1));

			while (currentDest < endDest)
			{
				*((uint32_t*)&buffer) = *((uint32_t*)currentSource);

				ind_lutd = LUT_INDEX(buffer[0], buffer[1], buffer[3]);
				ind_lutd2 = LUT_INDEX(buffer[2], buffer[1], buffer[3]);

				*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
				currentDest += 3;
				*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd2]));
				currentDest += 3;
				currentSource += 4;
			}
		}
		return;
	}


	if (pixelFormat == PixelFormat::RGB24 || pixelFormat == PixelFormat::XRGB)
	{
		for (int yDest = outputHeight - 1, ySource = _cropBottom; yDest >= 0; ++ySource, --yDest)
		{
			uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
			uint8_t* endDest = currentDest + destLineSize;
			uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource) + (((uint64_t)_cropLeft) * ((pixelFormat == PixelFormat::RGB24) ? 3 : 4)));

			while (currentDest < endDest)
			{
				if (pixelFormat == PixelFormat::RGB24)
				{
					if (lutBuffer != NULL)
					{						
						*((uint32_t*)&buffer) = *((uint32_t*)currentSource);
						currentSource += 3;
						ind_lutd = LUT_INDEX(buffer[2], buffer[1], buffer[0]);
					}
					else
					{
						*((uint32_t*)&buffer) = *((uint32_t*)currentSource);
						currentSource += 3;
						*currentDest++ = buffer[2];
						*currentDest++ = buffer[1];
						*currentDest++ = buffer[0];						
						continue;
					}
				}
				else if (pixelFormat == PixelFormat::XRGB)
				{
					if (lutBuffer != NULL)
					{
						*((uint32_t*)&buffer) = *((uint32_t*)currentSource);
						currentSource += 4;
						ind_lutd = LUT_INDEX(buffer[2], buffer[1], buffer[0]);
					}
					else
					{
						*((uint32_t*)&buffer) = *((uint32_t*)currentSource);
						currentSource += 4;
						*currentDest++ = buffer[2];
						*currentDest++ = buffer[1];
						*currentDest++ = buffer[0];
						continue;
					}
				}
				
				*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
				currentDest += 3;
			}
		}
		return;
	}

	if (pixelFormat == PixelFormat::I420)
	{
		int deltaU = lineLength * height;
		int deltaV = lineLength * height * 5 / 4;
		for (int yDest = 0, ySource = _cropTop; yDest < outputHeight; ++ySource, ++yDest)
		{
			uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
			uint8_t* endDest = currentDest + destLineSize;
			uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource) + ((uint64_t)_cropLeft));
			uint8_t* currentSourceU = (uint8_t*)data + deltaU + (ySource * lineLength / 2 + _cropLeft) / 2;
			uint8_t* currentSourceV = (uint8_t*)data + deltaV + (ySource * lineLength / 2 + _cropLeft) / 2;			

			while (currentDest < endDest)
			{
				*((uint16_t*)&buffer) = *((uint16_t*)currentSource);
				currentSource += 2;
				buffer[2] = *(currentSourceU++);
				buffer[3] = *(currentSourceV++);

				ind_lutd = LUT_INDEX(buffer[0], buffer[2], buffer[3]);
				ind_lutd2 = LUT_INDEX(buffer[1], buffer[2], buffer[3]);

				*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
				currentDest += 3;
				*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd2]));
				currentDest += 3;				
			}
		}
		return;
	}

	if (pixelFormat == PixelFormat::NV12)
	{
		int deltaU = lineLength * height;
		for (int yDest = 0, ySource = _cropTop; yDest < outputHeight; ++ySource, ++yDest)
		{
			uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
			uint8_t* endDest = currentDest + destLineSize;
			uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource) + ((uint64_t)_cropLeft));
			uint8_t* currentSourceU = (uint8_t*)data + deltaU + (((uint64_t)ySource/2) * lineLength) + ((uint64_t)_cropLeft);

			while (currentDest < endDest)
			{
				*((uint16_t*)&buffer) = *((uint16_t*)currentSource);
				currentSource += 2;
				*((uint16_t*)&(buffer[2])) = *((uint16_t*)currentSourceU);
				currentSourceU += 2;

				ind_lutd = LUT_INDEX(buffer[0], buffer[2], buffer[3]);
				ind_lutd2 = LUT_INDEX(buffer[1], buffer[2], buffer[3]);

				*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
				currentDest += 3;
				*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd2]));
				currentDest += 3;				
			}
		}
		return;
	}
}

#define LUT(source) \
{\
	memcpy(buffer, source, 3); \
	uint32_t ind_lutd = LUT_INDEX(buffer[0],buffer[1],buffer[2]);	\
	memcpy(source, (void *) &(lutBuffer[ind_lutd]),3); \
	source += 3;	\
}

void ImageResampler::applyLUT(unsigned char* _source, unsigned int width, unsigned int height, const uint8_t* lutBuffer, const int _hdrToneMappingEnabled)
{ 
	uint8_t buffer[4];

	if (lutBuffer != NULL && _hdrToneMappingEnabled)
	{
		unsigned int sizeX = (width * 10) / 100;
		unsigned int sizeY = (height * 25) / 100;

		for (unsigned int y = 0; y < height; y++)
		{
			unsigned char* startSource = _source + static_cast<size_t>(width) * 3 * y;
			unsigned char* endSource = startSource + static_cast<size_t>(width) * 3;

			if (_hdrToneMappingEnabled != 2 || y < sizeY || y > height - sizeY)
			{
				while (startSource < endSource)
					LUT(startSource);
			}
			else
			{
				for (unsigned int x = 0; x < sizeX; x++)
					LUT(startSource);

				startSource += (width - 2 * static_cast<size_t>(sizeX)) * 3;

				while (startSource < endSource)
					LUT(startSource);
			}
		}
	}
}

