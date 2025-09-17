#pragma once

#include <image/MemoryBuffer.h>
#include <image/ColorRgb.h>
#include <image/Image.h>
#include <utils/PixelFormat.h>

// some stuff for HDR tone mapping
#define LUT_INDEX(y,u,v) ((y + (u<<8) + (v<<16))*3)

class AutomaticToneMapping;

namespace FrameDecoder
{
	template<bool Quarter, bool UseToneMapping, bool UseAutomaticToneMapping>
	void processImageVector(
		int _cropLeft, int _cropRight, int _cropTop, int _cropBottom,
		const uint8_t* data, const uint8_t* dataUV, int width, int height, int lineLength,
		const PixelFormat pixelFormat, const uint8_t* lutBuffer,
		Image<ColorRgb>& outputImage, AutomaticToneMapping* automaticToneMapping);

	constexpr void (*dispatchProcessImageVector[2][2][2])(
		int cropLeft, int cropRight, int cropTop, int cropBottom,
		const uint8_t* data, const uint8_t* dataUV,
		int width, int height, int lineLength,
		PixelFormat pixelFormat, const uint8_t* lutBuffer,
		Image<ColorRgb>& outputImage,
		AutomaticToneMapping* automaticToneMapping) =
	{
		{   // Quarter = false
			{   // UseToneMapping = false
				&FrameDecoder::processImageVector<false, false, false>,
				&FrameDecoder::processImageVector<false, false, true>
			},
			{   // UseToneMapping = true
				&FrameDecoder::processImageVector<false, true, false>,
				&FrameDecoder::processImageVector<false, true, true>
			}
		},
		{   // Quarter = true
			{   // UseToneMapping = false
				&FrameDecoder::processImageVector<true, false, false>,
				&FrameDecoder::processImageVector<true, false, true>
			},
			{   // UseToneMapping = true
				&FrameDecoder::processImageVector<true, true, false>,
				&FrameDecoder::processImageVector<true, true, true>
			}
		}
	};

	void processSystemImageBGRA(Image<ColorRgb>& image, int targetSizeX, int targetSizeY,
									   int startX, int startY,
									   uint8_t* source, int _actualWidth, int _actualHeight,
									   int division, uint8_t* _lutBuffer, int lineSize = 0);

	void processSystemImageBGR(Image<ColorRgb>& image, int targetSizeX, int targetSizeY,
										int startX, int startY,
										uint8_t* source, int _actualWidth, int _actualHeight,
										int division, uint8_t* _lutBuffer, int lineSize = 0);

	void processSystemImageBGR16(Image<ColorRgb>& image, int targetSizeX, int targetSizeY,
										int startX, int startY,
										uint8_t* source, int _actualWidth, int _actualHeight,
										int division, uint8_t* _lutBuffer, int lineSize = 0);

	void processSystemImageRGBA(Image<ColorRgb>& image, int targetSizeX, int targetSizeY,
									   int startX, int startY,
									   uint8_t* source, int _actualWidth, int _actualHeight,
									   int division, uint8_t* _lutBuffer, int lineSize = 0);

	void processSystemImagePQ10(Image<ColorRgb>& image, int targetSizeX, int targetSizeY,
									   int startX, int startY,
									   uint8_t* source, int _actualWidth, int _actualHeight,
									   int division, uint8_t* _lutBuffer, int lineSize = 0);

	void applyLUT(uint8_t* _source, unsigned int width, unsigned int height, const uint8_t* lutBuffer, const int _hdrToneMappingEnabled);
};

