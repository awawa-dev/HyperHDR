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
	size_t ind_lutd = (LUTD_R_STRIDE(red) + LUTD_G_STRIDE(green) + LUTD_B_STRIDE(blue));	\
	*(dest++) = lutBuffer[ind_lutd + LUTD_C_STRIDE(0)];	\
	*(dest++) = lutBuffer[ind_lutd + LUTD_C_STRIDE(1)];	\
	*(dest++) = lutBuffer[ind_lutd + LUTD_C_STRIDE(2)];	\
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

void ImageResampler::processImage(const uint8_t * data, int width, int height, int lineLength, PixelFormat pixelFormat, unsigned char *lutBuffer, Image<ColorRgb> &outputImage) const
{	
	uint8_t _red, _green, _blue;
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
	
	// fast copy
	if (_cropLeft==0 && cropRight == 0 && _cropTop ==0 && cropBottom == 0 && _verticalDecimation==1 && _horizontalDecimation==1)
	{
		uint8_t 	*destMemory  = (uint8_t *)outputImage.memptr();
		int 		destLineSize = outputImage.width()*3;		
		
		if (pixelFormat == PixelFormat::YUYV)
		{
			for(int y=0; y < height; y++)
			{
				int index = lineLength * y;
				uint8_t *currentDest = destMemory + destLineSize * y;
				
				for (int x=0; x < (width >> 1); x++)
				{
					_red = data[index];  	  // Y
					_green = data[index+1];  // U
					_blue = data[index+3];   // V
					// LUT mapping
					LUT(currentDest, _red, _green, _blue);
					
					_red = data[index+2];	  // Y2					
					// LUT mapping
					LUT(currentDest, _red, _green, _blue);
					index += 4;
				}				
			}
			return;
		}
		else if (pixelFormat == PixelFormat::UYVY)
		{
			for(int y=0; y < height; y++)
			{
				int index = lineLength * y;
				uint8_t *currentDest = destMemory + destLineSize * y;
				
				for (int x=0; x < (width >> 1); x++)
				{
					_red = data[index+1];    // Y
					_green = data[index];    // U
					_blue = data[index+2];   // V
					// LUT mapping
					LUT(currentDest, _red, _green, _blue);
					
					_red = data[index+3];	  // Y2
					// LUT mapping
					LUT(currentDest, _red, _green, _blue);
					index += 4;
				}				
			}
			return;
		}
	}

	// calculate the output size
	int outputWidth = (width - _cropLeft - cropRight - (_horizontalDecimation >> 1) + _horizontalDecimation - 1) / _horizontalDecimation;
	int outputHeight = (height - _cropTop - cropBottom - (_verticalDecimation >> 1) + _verticalDecimation - 1) / _verticalDecimation;

	if (outputImage.width() != unsigned(outputWidth) || outputImage.height() != unsigned(outputHeight))
		outputImage.resize(outputWidth, outputHeight);

	uint8_t 	*destMemory  = (uint8_t *)outputImage.memptr();
	int 		destLineSize = outputImage.width()*3;	
	
	for (int yDest = 0, ySource = _cropTop + (_verticalDecimation >> 1); yDest < outputHeight; ySource += _verticalDecimation, ++yDest)
	{
		uint8_t *currentDest = destMemory + destLineSize * yDest;	
		int 	lineLength_ySource = lineLength * ySource;

		for (int xDest = 0, xSource = _cropLeft + (_horizontalDecimation >> 1); xDest < outputWidth; xSource += _horizontalDecimation, ++xDest)
		{			
			switch (pixelFormat)
			{
				case PixelFormat::UYVY:
				{
					int index = lineLength_ySource + (xSource << 1);
					_red = data[index+1]; 					       // Y
					_green = ((xSource&1) == 0) ? data[index  ] : data[index-2]; // U
					_blue = ((xSource&1) == 0) ? data[index+2] : data[index  ];  // V	
					// LUT mapping
					LUT(currentDest, _red, _green, _blue);
				}
				break;
				case PixelFormat::YUYV:
				{
					int index = lineLength_ySource + (xSource << 1);
					_red = data[index];						// Y
					_green = ((xSource&1) == 0) ? data[index+1] : data[index-1];  // U
					_blue = ((xSource&1) == 0) ? data[index+3] : data[index+1];   // V
					// LUT mapping
					LUT(currentDest, _red, _green, _blue);
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
