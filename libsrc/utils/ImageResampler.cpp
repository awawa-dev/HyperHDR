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

#define LUT(dest, red, green, blue) \
{\
	uint32_t ind_lutd = LUT_INDEX(red, green, blue);	\
	*(dest++) = lutBuffer[ind_lutd];	\
	*(dest++) = lutBuffer[ind_lutd + 1];	\
	*(dest++) = lutBuffer[ind_lutd + 2];	\
}

#define LUTIF(dest, red, green, blue) \
{\
	if (lutBuffer == NULL)\
	{\
		*(dest++) = red;	\
		*(dest++) = green;	\
		*(dest++) = blue;	\
	}\
	else\
		LUT(dest, red, green, blue);\
}

void ImageResampler::processImage(
	int _cropLeft, int _cropRight, int _cropTop, int _cropBottom,
	const uint8_t * data, int width, int height, int lineLength, PixelFormat pixelFormat, const uint8_t *lutBuffer, Image<ColorRgb> &outputImage)
{	
	uint8_t _red, _green, _blue, buffer[4];	
			
	// validate format
	if (pixelFormat!=PixelFormat::UYVY && pixelFormat!=PixelFormat::YUYV &&
	    pixelFormat!=PixelFormat::BGR16 && pixelFormat!=PixelFormat::BGR24 &&
	    pixelFormat!=PixelFormat::RGB32 && pixelFormat!=PixelFormat::BGR32)
	{
		Error(Logger::getInstance("ImageResampler"), "Invalid pixel format given");
		return;
	}
	
	// validate format LUT
	if ((pixelFormat==PixelFormat::UYVY || pixelFormat==PixelFormat::YUYV) && lutBuffer == NULL)
	{
		Error(Logger::getInstance("ImageResampler"), "Missing LUT table for YUV colorspace");
		return;
	}
	
	// sanity check, odd values doesnt work for yuv either way
	_cropLeft = (_cropLeft>>1)<<1;
	_cropRight = (_cropRight>>1)<<1;				

	// calculate the output size
	int outputWidth = (width - _cropLeft - _cropRight);
	int outputHeight = (height - _cropTop - _cropBottom);

	//if (outputImage.width() != unsigned(outputWidth) || outputImage.height() != unsigned(outputHeight))
	outputImage.resize(outputWidth, outputHeight);

	uint8_t 	*destMemory  = (uint8_t *)outputImage.memptr();
	int 		destLineSize = outputImage.width()*3;	
	
	
	if (pixelFormat == PixelFormat::YUYV || pixelFormat == PixelFormat::UYVY)
	{
		for (int yDest = 0, ySource = _cropTop; yDest < outputHeight; ++ySource, ++yDest)
		{
			uint8_t *currentDest = destMemory + destLineSize * yDest;	
			uint8_t *endDest = currentDest + destLineSize;
			uint8_t *currentSource = (uint8_t *) data +  (lineLength * ySource) + (_cropLeft<<1);
			uint32_t ind_lutd, ind_lutd2;
			while(currentDest<endDest)
			{
				memcpy(&buffer,currentSource,4);			
				if (pixelFormat == PixelFormat::YUYV)
				{
					ind_lutd  = LUT_INDEX(buffer[0], buffer[1], buffer[3]);
					ind_lutd2 = LUT_INDEX(buffer[2], buffer[1], buffer[3]);
				}
				else if (pixelFormat == PixelFormat::UYVY)
				{					
					ind_lutd  = LUT_INDEX(buffer[1], buffer[0], buffer[2]);
					ind_lutd2 = LUT_INDEX(buffer[3], buffer[0], buffer[2]);
				}
				memcpy(currentDest, (const void*)&(lutBuffer[ind_lutd]),3);
				memcpy(currentDest+3, (const void*)&(lutBuffer[ind_lutd2]),3);
				currentDest+=6;
				currentSource+=4;
			}		
		}
		return;
	}
	
	
	
	for (int yDest = 0, ySource = _cropTop; yDest < outputHeight; ySource++, ++yDest)
	{
		uint8_t *currentDest = destMemory + destLineSize * yDest;	
		int 	lineLength_ySource = lineLength * ySource;
		
		for (int xDest = 0, xSource = _cropLeft; xDest < outputWidth; xSource++, ++xDest)
		{					
			switch (pixelFormat)
			{				
				case PixelFormat::BGR16:
				{
					int index = lineLength_ySource + (xSource << 1);										
					_red   = (data[index+1] & 0xF8); 					       //red
					_green = ((((data[index+1] & 0x7) << 3) | (data[index] & 0xE0) >> 5) << 2); //green
					_blue  = ((data[index] & 0x1f) << 3);					       //blue					
					// LUTIF mapping
					LUTIF(currentDest, _red, _green, _blue);
				}
				break;
				case PixelFormat::BGR24:
				{
					int index = lineLength_ySource + (xSource << 1) + xSource;
					_red   = data[index+2]; // red
					_green = data[index+1]; // green
					_blue  = data[index  ]; // blue				
					// LUTIF mapping
					LUTIF(currentDest, _red, _green, _blue);					
				}
				break;
				case PixelFormat::RGB32:
				{
					int index = lineLength_ySource + (xSource << 2);
					_red   = data[index  ]; // red
					_green = data[index+1]; // green
					_blue  = data[index+2]; // blue							
					// LUTIF mapping
					LUTIF(currentDest, _red, _green, _blue);

				}
				break;
				case PixelFormat::BGR32:
				{
					int index = lineLength_ySource + (xSource << 2);										
					_red   = data[index+2]; // red				
					_green = data[index+1]; // green
					_blue  = data[index  ]; // blue
					// LUTIF mapping
					LUTIF(currentDest, _red, _green, _blue);
				}
				break;				
				default:
				break;
			}						
		}
	}
}

