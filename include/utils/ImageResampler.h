#pragma once

#include <utils/VideoMode.h>
#include <utils/PixelFormat.h>
#include <utils/Image.h>
#include <utils/ColorRgb.h>


// some stuff for HDR tone mapping
#define LUT_INDEX(y,u,v) ((y + (u<<8) + (v<<16))*3)

class ImageResampler
{
public:
	ImageResampler();
	~ImageResampler();

	void setHorizontalPixelDecimation(int decimator);
	void setVerticalPixelDecimation(int decimator);
	void setCropping(int cropLeft, int cropRight, int cropTop, int cropBottom);
	void setVideoMode(VideoMode mode);
	void processImage(const uint8_t * data, int width, int height, int lineLength, PixelFormat pixelFormat, Image<ColorRgb> &outputImage) const;
	static void processImage(
		VideoMode _mode,
		int _cropLeft, int _cropRight, int _cropTop, int _cropBottom,			
		const uint8_t * data, int width, int height, int lineLength, PixelFormat pixelFormat, unsigned char *lutBuffer, Image<ColorRgb> &outputImage);

private:
	int _horizontalDecimation;
	int _verticalDecimation;
	int _cropLeft;
	int _cropRight;
	int _cropTop;
	int _cropBottom;
	VideoMode _videoMode;
};

