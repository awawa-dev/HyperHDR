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
	, _videoMode(VideoMode::VIDEO_2D)
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

void ImageResampler::setVideoMode(VideoMode mode)
{
	_videoMode = mode;
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
	VideoMode _videoMode,
	int _cropLeft, int _cropRight, int _cropTop, int _cropBottom,
	int _horizontalDecimation, int _verticalDecimation,
	const uint8_t * data, int width, int height, int lineLength, PixelFormat pixelFormat, unsigned char *lutBuffer, Image<ColorRgb> &outputImage)
{	
	uint8_t _red, _green, _blue, _Y, _U, _V;
	int     cropRight  = _cropRight;
	int     cropBottom = _cropBottom;
			
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
	
	// handle 3D mode
	switch (_videoMode)
	{
		case VideoMode::VIDEO_3DSBS:
			cropRight = width >> 1;
			break;
		case VideoMode::VIDEO_3DTAB:
			cropBottom = width >> 1;
			break;
		default:
			break;
	}	
	
	// sanity check, odd values doesnt work for yuv either way
	_cropLeft = (_cropLeft>>1)<<1;
	cropRight = (cropRight>>1)<<1;				

	// calculate the output size
	int outputWidth = (width - _cropLeft - cropRight - (_horizontalDecimation >> 1) + _horizontalDecimation - 1) / _horizontalDecimation;
	int outputHeight = (height - _cropTop - cropBottom - (_verticalDecimation >> 1) + _verticalDecimation - 1) / _verticalDecimation;

	if (outputImage.width() != unsigned(outputWidth) || outputImage.height() != unsigned(outputHeight))
		outputImage.resize(outputWidth, outputHeight);

	uint8_t 	*destMemory  = (uint8_t *)outputImage.memptr();
	int 		destLineSize = outputImage.width()*3;	
	
	
	if ((pixelFormat == PixelFormat::YUYV || pixelFormat == PixelFormat::UYVY) && _horizontalDecimation == 1 && _verticalDecimation == 1)
	{
		for (int yDest = 0, ySource = _cropTop; yDest < outputHeight; ++ySource, ++yDest)
		{
			uint8_t *currentDest = destMemory + destLineSize * yDest;	
			uint8_t *endDest = currentDest + destLineSize - (cropRight<<1);
			uint8_t *currentSource = (uint8_t *) data +  (lineLength * ySource) + (_cropLeft<<1);
			
			while(currentDest<endDest)
			{
				if (pixelFormat == PixelFormat::YUYV)
				{
					uint8_t buffer[4];
					memcpy(&buffer,currentSource,4);
					uint32_t ind_lutd  = LUT_INDEX(buffer[0], buffer[1], buffer[3]);
					uint32_t ind_lutd2 = LUT_INDEX(buffer[2], buffer[1], buffer[3]);
					memcpy(currentDest, (const void*)&(lutBuffer[ind_lutd]),3);
					memcpy(currentDest+3, (const void*)&(lutBuffer[ind_lutd2]),3);
					currentDest+=6;
					currentSource+=4;					
				}
				else if (pixelFormat == PixelFormat::UYVY)
				{
					uint8_t buffer[4];
					memcpy(&buffer,currentSource,4);
					uint32_t ind_lutd  = LUT_INDEX(buffer[1], buffer[0], buffer[2]);
					uint32_t ind_lutd2 = LUT_INDEX(buffer[3], buffer[0], buffer[2]);
					memcpy(currentDest, (const void*)&(lutBuffer[ind_lutd]),3);
					memcpy(currentDest+3, (const void*)&(lutBuffer[ind_lutd2]),3);
					currentDest+=6;
					currentSource+=4;					
				}
			}		
		}
		return;
	}
	
	
	
	for (int yDest = 0, ySource = _cropTop + (_verticalDecimation >> 1); yDest < outputHeight; ySource += _verticalDecimation, ++yDest)
	{
		uint8_t *currentDest = destMemory + destLineSize * yDest;	
		int 	lineLength_ySource = lineLength * ySource;
		
		for (int xDest = 0, xSource = _cropLeft + (_horizontalDecimation >> 1); xDest < outputWidth; xSource += _horizontalDecimation, ++xDest)
		{	
			// i hate it...
			if (_horizontalDecimation == 1)		
			{	
								
			}		
				
			switch (pixelFormat)
			{
				case PixelFormat::UYVY:
				{
					int index = lineLength_ySource + (xSource << 1);
					_Y = data[index+1]; 					   // Y
					_U = ((xSource&1) == 0) ? data[index  ] : data[index-2]; // U
					_V = ((xSource&1) == 0) ? data[index+2] : data[index  ]; // V	
					// LUT mapping
					LUT(currentDest, _Y, _U, _V);
				}
				break;
				case PixelFormat::YUYV:
				{
					int index = lineLength_ySource + (xSource << 1);
					_Y = data[index];					    // Y
					_U = ((xSource&1) == 0) ? data[index+1] : data[index-1];  // U
					_V = ((xSource&1) == 0) ? data[index+3] : data[index+1];  // V
					// LUT mapping
					LUT(currentDest, _Y, _U, _V);
				}
				break;
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

// the old way. but it is used in ther modules so I leave it as it is
void ImageResampler::processImage(const uint8_t * data, int width, int height, int lineLength, PixelFormat pixelFormat, Image<ColorRgb> &outputImage) const
{
	int cropRight  = _cropRight;
	int cropBottom = _cropBottom;

	// handle 3D mode
	switch (_videoMode)
	{
	case VideoMode::VIDEO_3DSBS:
		cropRight = width >> 1;
		break;
	case VideoMode::VIDEO_3DTAB:
		cropBottom = width >> 1;
		break;
	default:
		break;
	}

	// calculate the output size
	int outputWidth = (width - _cropLeft - cropRight - (_horizontalDecimation >> 1) + _horizontalDecimation - 1) / _horizontalDecimation;
	int outputHeight = (height - _cropTop - cropBottom - (_verticalDecimation >> 1) + _verticalDecimation - 1) / _verticalDecimation;

	outputImage.resize(outputWidth, outputHeight);

	for (int yDest = 0, ySource = _cropTop + (_verticalDecimation >> 1); yDest < outputHeight; ySource += _verticalDecimation, ++yDest)
	{
		int yOffset = lineLength * ySource;

		for (int xDest = 0, xSource = _cropLeft + (_horizontalDecimation >> 1); xDest < outputWidth; xSource += _horizontalDecimation, ++xDest)
		{
			ColorRgb & rgb = outputImage(xDest, yDest);

			switch (pixelFormat)
			{
				case PixelFormat::UYVY:
				{
					int index = yOffset + (xSource << 1);
					uint8_t y = data[index+1];
					uint8_t u = ((xSource&1) == 0) ? data[index  ] : data[index-2];
					uint8_t v = ((xSource&1) == 0) ? data[index+2] : data[index  ];
					ColorSys::yuv2rgb(y, u, v, rgb.red, rgb.green, rgb.blue);
				}
				break;
				case PixelFormat::YUYV:
				{
					int index = yOffset + (xSource << 1);
					uint8_t y = data[index];
					uint8_t u = ((xSource&1) == 0) ? data[index+1] : data[index-1];
					uint8_t v = ((xSource&1) == 0) ? data[index+3] : data[index+1];
					ColorSys::yuv2rgb(y, u, v, rgb.red, rgb.green, rgb.blue);
				}
				break;
				case PixelFormat::BGR16:
				{
					int index = yOffset + (xSource << 1);
					rgb.blue  = (data[index] & 0x1f) << 3;
					rgb.green = (((data[index+1] & 0x7) << 3) | (data[index] & 0xE0) >> 5) << 2;
					rgb.red   = (data[index+1] & 0xF8);
				}
				break;
				case PixelFormat::BGR24:
				{
					int index = yOffset + (xSource << 1) + xSource;
					rgb.blue  = data[index  ];
					rgb.green = data[index+1];
					rgb.red   = data[index+2];
				}
				break;
				case PixelFormat::RGB32:
				{
					int index = yOffset + (xSource << 2);
					rgb.red   = data[index  ];
					rgb.green = data[index+1];
					rgb.blue  = data[index+2];
				}
				break;
				case PixelFormat::BGR32:
				{
					int index = yOffset + (xSource << 2);
					rgb.blue  = data[index  ];
					rgb.green = data[index+1];
					rgb.red   = data[index+2];
				}
				break;
#ifdef HAVE_JPEG_DECODER
				case PixelFormat::MJPEG:
				break;
#endif
				case PixelFormat::NO_CHANGE:
					Error(Logger::getInstance("ImageResampler"), "Invalid pixel format given");
				break;
			}
		}
	}
}
