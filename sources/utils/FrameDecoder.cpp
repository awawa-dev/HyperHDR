/* FrameDecoder.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2025 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/HyperHDR
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
*/


#include <utils/Logger.h>
#include <utils/FrameDecoder.h>

#include <lut-calibrator/ColorSpace.h>
#include <lut-calibrator/VectorHelper.h>
#include <atomic>
#include <mutex>

namespace FrameDecoderUtils
{
	std::atomic<bool> initialized(false);
	std::vector<uint8_t> lutP010_y;
	std::vector<uint8_t> lutP010_uv;

	constexpr double signalBreakP010 = 0.91;
	constexpr double signalBreakChromaP010 = 0.75;

	static double packChromaP010(double x)
	{
		constexpr double pi2 = M_PI / 2.0;
		if (x < 0.0)
		{
			return 0.0;
		}
		else if (x <= 0.5)
		{
			return x * 1.5;
		}
		else if (x <= 1)
		{			
			return std::sin(pi2 * ((x - 0.5) / 0.5)) * (1 - signalBreakChromaP010) + signalBreakChromaP010;
		}
		return 1;
	};

	double unpackChromaP010(double x)
	{
		constexpr double pi2 = M_PI / 2.0;
		if (x < 0.0)
		{
			return 0.0;
		}
		else if (x <= signalBreakChromaP010)
		{
			x /= 1.5;
			return x;
		}
		else if (x <= 1)
		{			
			x = (x - signalBreakChromaP010) / (1.0 - signalBreakChromaP010);
			x = std::asin(x);
			x = x * 0.5 / pi2  + 0.5;
			return x;
		}

		return 1;
	};

	static double packLuminanceP010(double x)
	{
		constexpr double pi2 = M_PI / 2.0;
		if (x < 0.0)
		{
			return 0.0;
		}
		else if (x <= 0.7)
		{
			return x * 1.3;
		}
		else if (x <= 1)
		{
			return std::sin(pi2 * ((x - 0.7) / 0.3)) * (1 - signalBreakP010) + signalBreakP010;
		}
		return 1;
	};

	double unpackLuminanceP010(double x)
	{
		constexpr double pi2 = M_PI / 2.0;
		if (x < 0.0)
		{
			return 0.0;
		}
		else if (x <= signalBreakP010)
		{
			return x / 1.3;
		}
		else if (x <= 1)
		{
			x = (x - signalBreakP010) / (1.0 - signalBreakP010);
			x = std::asin(x);
			x = x * 0.3 / pi2 + 0.7;
			return x;
		}

		return 1;
	};


	static void initP010()
	{
		static std::mutex lockerP010;
		std::lock_guard<std::mutex> locker(lockerP010);

		if (FrameDecoderUtils::initialized)
			return;

		lutP010_y.resize(1024);
		lutP010_uv.resize(1024);

		for (int i = 0; i < static_cast<int>(lutP010_y.size()); ++i)
		{
			constexpr int sourceRange = 1023;
			const double sourceValue = std::min(std::max(i, 0), sourceRange)/static_cast<double>(sourceRange);
			double val = packLuminanceP010(sourceValue);
			lutP010_y[i] = std::lround(val * 255.0);

			/*
			double unpack = unpackLuminanceP010(val);
			double delta = sourceValue - unpack;
			if (std::abs(delta) > 0.0000001)
			{
				bool error = true;
			}
			*/
		}


		for (int i = 0; i < static_cast<int>(lutP010_uv.size()); ++i)
		{
			constexpr int sourceRange = (960 - 64) / 2;
			const int current = std::abs(i - 512);
			const double sourceValue = std::min(current, sourceRange) / static_cast<double>(sourceRange);
			double val = packChromaP010(sourceValue);
			lutP010_uv[i] = std::max(std::min(128 + std::lround(((i < 512) ? -val : val) * 128.0), 255l), 0l);;

			/*
			double unpack = unpackChromaP010(val);
			double delta = sourceValue - unpack;
			if (std::abs(delta) > 0.0000001)
			{
				bool error = true;
			}
			*/
		}

		FrameDecoderUtils::initialized = true;
	};	
};

using namespace FrameDecoderUtils;


void FrameDecoder::processImage(
	int _cropLeft, int _cropRight, int _cropTop, int _cropBottom,
	const uint8_t* data, const uint8_t* dataUV, int width, int height, int lineLength,
	const PixelFormat pixelFormat, const uint8_t* lutBuffer, Image<ColorRgb>& outputImage, bool toneMapping)
{
	uint32_t ind_lutd, ind_lutd2;
	uint8_t  buffer[8];

	// validate format
	if (pixelFormat != PixelFormat::YUYV && pixelFormat != PixelFormat::UYVY &&
		pixelFormat != PixelFormat::XRGB && pixelFormat != PixelFormat::RGB24 &&
		pixelFormat != PixelFormat::I420 && pixelFormat != PixelFormat::NV12 && pixelFormat != PixelFormat::P010 && pixelFormat != PixelFormat::MJPEG)
	{
		Error(Logger::getInstance("FrameDecoder"), "Invalid pixel format given");
		return;
	}

	// validate format LUT
	if ((pixelFormat == PixelFormat::YUYV || pixelFormat == PixelFormat::UYVY || pixelFormat == PixelFormat::I420 || pixelFormat == PixelFormat::MJPEG ||
		pixelFormat == PixelFormat::NV12 || pixelFormat == PixelFormat::P010) && lutBuffer == NULL)
	{
		Error(Logger::getInstance("FrameDecoder"), "Missing LUT table for YUV colorspace");
		return;
	}

	// sanity check, odd values doesnt work for yuv either way
	_cropLeft = (_cropLeft >> 1) << 1;
	_cropRight = (_cropRight >> 1) << 1;

	// calculate the output size
	int outputWidth = (width - _cropLeft - _cropRight);
	int outputHeight = (height - _cropTop - _cropBottom);

	outputImage.resize(outputWidth, outputHeight);
	outputImage.setOriginFormat(pixelFormat);

	uint8_t* destMemory = outputImage.rawMem();
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

	if (pixelFormat == PixelFormat::UYVY)
	{
		for (int yDest = 0, ySource = _cropTop; yDest < outputHeight; ++ySource, ++yDest)
		{
			uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
			uint8_t* endDest = currentDest + destLineSize;
			uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource) + (((uint64_t)_cropLeft) << 1));

			while (currentDest < endDest)
			{
				*((uint32_t*)&buffer) = *((uint32_t*)currentSource);

				ind_lutd = LUT_INDEX(buffer[1], buffer[0], buffer[2]);
				ind_lutd2 = LUT_INDEX(buffer[3], buffer[0], buffer[2]);

				*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
				currentDest += 3;
				*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd2]));
				currentDest += 3;
				currentSource += 4;
			}
		}
		return;
	}

	if (pixelFormat == PixelFormat::RGB24)
	{
		for (int yDest = outputHeight - 1, ySource = _cropBottom; yDest >= 0; ++ySource, --yDest)
		{
			uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
			uint8_t* endDest = currentDest + destLineSize;
			uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource) + (((uint64_t)_cropLeft) * 3));

			if (lutBuffer == NULL)
				currentSource += 2;

			while (currentDest < endDest)
			{
				if (lutBuffer != NULL)
				{
					*((uint32_t*)&buffer) = *((uint32_t*)currentSource);
					currentSource += 3;
					ind_lutd = LUT_INDEX(buffer[2], buffer[1], buffer[0]);
					*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
					currentDest += 3;
				}
				else
				{
					*currentDest++ = *currentSource--;
					*currentDest++ = *currentSource--;
					*currentDest++ = *currentSource;
					currentSource += 5;
				}
			}
		}
		return;
	}

	if (pixelFormat == PixelFormat::XRGB)
	{
		for (int yDest = outputHeight - 1, ySource = _cropBottom; yDest >= 0; ++ySource, --yDest)
		{
			uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
			uint8_t* endDest = currentDest + destLineSize;
			uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource) + (((uint64_t)_cropLeft) * 4));

			if (lutBuffer == NULL)
				currentSource += 2;

			while (currentDest < endDest)
			{
				if (lutBuffer != NULL)
				{
					*((uint32_t*)&buffer) = *((uint32_t*)currentSource);
					currentSource += 4;
					ind_lutd = LUT_INDEX(buffer[2], buffer[1], buffer[0]);
					*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
					currentDest += 3;
				}
				else
				{
					*currentDest++ = *currentSource--;
					*currentDest++ = *currentSource--;
					*currentDest++ = *currentSource;
					currentSource += 6;
				}
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
			uint8_t* currentSourceU = (uint8_t*)data + deltaU + ((((uint64_t)ySource / 2) * lineLength) + ((uint64_t)_cropLeft)) / 2;
			uint8_t* currentSourceV = (uint8_t*)data + deltaV + ((((uint64_t)ySource / 2) * lineLength) + ((uint64_t)_cropLeft)) / 2;

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

	if (pixelFormat == PixelFormat::MJPEG)
	{
		int deltaU = lineLength * height;
		int deltaV = lineLength * height * 6 / 4;
		for (int yDest = 0, ySource = _cropTop; yDest < outputHeight; ++ySource, ++yDest)
		{
			uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
			uint8_t* endDest = currentDest + destLineSize;
			uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource) + ((uint64_t)_cropLeft));
			uint8_t* currentSourceU = (uint8_t*)data + deltaU + ((((uint64_t)ySource) * lineLength) + ((uint64_t)_cropLeft)) / 2;
			uint8_t* currentSourceV = (uint8_t*)data + deltaV + ((((uint64_t)ySource) * lineLength) + ((uint64_t)_cropLeft)) / 2;

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

	if (pixelFormat == PixelFormat::P010)
	{
		uint16_t p010[2] = {};

		if (!FrameDecoderUtils::initialized)
		{
			initP010();
		}

		auto deltaUV = (dataUV != nullptr) ? (uint8_t*)dataUV : (uint8_t*)data + lineLength * height;
		for (int yDest = 0, ySource = _cropTop; yDest < outputHeight; ++ySource, ++yDest)
		{
			uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
			uint8_t* endDest = currentDest + destLineSize;
			uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource) + ((uint64_t)_cropLeft));
			uint8_t* currentSourceUV = deltaUV + (((uint64_t)ySource / 2) * lineLength) + ((uint64_t)_cropLeft);

			while (currentDest < endDest)
			{
				memcpy(((uint32_t*)&p010), ((uint32_t*)currentSource), 4);
				if (toneMapping)
				{
					buffer[0] = lutP010_y[p010[0] >> 6];
					buffer[1] = lutP010_y[p010[1] >> 6];
				}
				else
				{
					buffer[0] = p010[0] >> 8;
					buffer[1] = p010[1] >> 8;
				}

				currentSource += 4;
				memcpy(((uint32_t*)&p010), ((uint32_t*)currentSourceUV), 4);
				if (toneMapping)
				{
					buffer[2] = lutP010_uv[p010[0] >> 6];
					buffer[3] = lutP010_uv[p010[1] >> 6];
				}
				else
				{
					buffer[2] = p010[0] >> 8;
					buffer[3] = p010[1] >> 8;
				}

				currentSourceUV += 4;

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
		auto deltaUV = (dataUV != nullptr) ? (uint8_t*)dataUV : (uint8_t*)data + lineLength * height;
		for (int yDest = 0, ySource = _cropTop; yDest < outputHeight; ++ySource, ++yDest)
		{
			uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
			uint8_t* endDest = currentDest + destLineSize;
			uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource) + ((uint64_t)_cropLeft));
			uint8_t* currentSourceUV = deltaUV + (((uint64_t)ySource / 2) * lineLength) + ((uint64_t)_cropLeft);

			while (currentDest < endDest)
			{
				*((uint16_t*)&buffer) = *((uint16_t*)currentSource);
				currentSource += 2;
				*((uint16_t*)&(buffer[2])) = *((uint16_t*)currentSourceUV);
				currentSourceUV += 2;

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

void FrameDecoder::processQImage(
	const uint8_t* data, const uint8_t* dataUV, int width, int height, int lineLength,
	const PixelFormat pixelFormat, const uint8_t* lutBuffer, Image<ColorRgb>& outputImage, bool toneMapping, AutomaticToneMapping* automaticToneMapping)
{
	uint32_t ind_lutd;
	uint8_t  buffer[8];

	// validate format
	if (pixelFormat != PixelFormat::YUYV && pixelFormat != PixelFormat::UYVY &&
		pixelFormat != PixelFormat::XRGB && pixelFormat != PixelFormat::RGB24 &&
		pixelFormat != PixelFormat::I420 && pixelFormat != PixelFormat::NV12 && pixelFormat != PixelFormat::P010)
	{
		Error(Logger::getInstance("FrameDecoder"), "Invalid pixel format given");
		return;
	}

	// validate format LUT
	if ((pixelFormat == PixelFormat::YUYV || pixelFormat == PixelFormat::UYVY || pixelFormat == PixelFormat::I420 ||
		pixelFormat == PixelFormat::NV12 || pixelFormat == PixelFormat::P010) && lutBuffer == NULL)
	{
		Error(Logger::getInstance("FrameDecoder"), "Missing LUT table for YUV colorspace");
		return;
	}

	// calculate the output size
	int outputWidth = (width) >> 1;
	int outputHeight = (height) >> 1;

	outputImage.resize(outputWidth, outputHeight);

	uint8_t* destMemory = outputImage.rawMem();
	int 		destLineSize = outputImage.width() * 3;


	if (pixelFormat == PixelFormat::YUYV && automaticToneMapping == nullptr)
	{
		for (int yDest = 0, ySource = 0; yDest < outputHeight; ySource += 2, ++yDest)
		{
			uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
			uint8_t* endDest = currentDest + destLineSize;
			uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource));

			while (currentDest < endDest)
			{
				*((uint32_t*)&buffer) = *((uint32_t*)currentSource);

				ind_lutd = LUT_INDEX(buffer[0], buffer[1], buffer[3]);

				*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
				currentDest += 3;
				currentSource += 4;
			}
		}
		return;
	}
	if (pixelFormat == PixelFormat::YUYV && automaticToneMapping != nullptr)
	{
		for (int yDest = 0, ySource = 0; yDest < outputHeight; ySource += 2, ++yDest)
		{
			uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
			uint8_t* endDest = currentDest + destLineSize;
			uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource));

			while (currentDest < endDest)
			{
				*((uint32_t*)&buffer) = *((uint32_t*)currentSource);

				ind_lutd = LUT_INDEX((automaticToneMapping->checkY(buffer[0])), (automaticToneMapping->checkU(buffer[1])), (automaticToneMapping->checkV(buffer[3])));

				*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
				currentDest += 3;
				currentSource += 4;
			}
		}
		automaticToneMapping->finilize();
		return;
	}

	if (pixelFormat == PixelFormat::UYVY)
	{
		for (int yDest = 0, ySource = 0; yDest < outputHeight; ySource += 2, ++yDest)
		{
			uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
			uint8_t* endDest = currentDest + destLineSize;
			uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource));

			while (currentDest < endDest)
			{
				*((uint32_t*)&buffer) = *((uint32_t*)currentSource);

				ind_lutd = LUT_INDEX(buffer[1], buffer[0], buffer[2]);

				*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
				currentDest += 3;
				currentSource += 4;
			}
		}
		return;
	}

	if (pixelFormat == PixelFormat::RGB24)
	{
		for (int yDest = outputHeight - 1, ySource = 0; yDest >= 0; ySource += 2, --yDest)
		{
			uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
			uint8_t* endDest = currentDest + destLineSize;
			uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource));

			if (lutBuffer == NULL)
				currentSource += 2;

			while (currentDest < endDest)
			{
				if (lutBuffer != NULL)
				{
					*((uint32_t*)&buffer) = *((uint32_t*)currentSource);
					currentSource += 6;
					ind_lutd = LUT_INDEX(buffer[2], buffer[1], buffer[0]);
					*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
					currentDest += 3;
				}
				else
				{
					*currentDest++ = *currentSource--;
					*currentDest++ = *currentSource--;
					*currentDest++ = *currentSource;
					currentSource += 8;
				}
			}
		}
		return;
	}

	if (pixelFormat == PixelFormat::XRGB)
	{
		for (int yDest = outputHeight - 1, ySource = 0; yDest >= 0; ySource += 2, --yDest)
		{
			uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
			uint8_t* endDest = currentDest + destLineSize;
			uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource));

			if (lutBuffer == NULL)
				currentSource += 2;

			while (currentDest < endDest)
			{
				if (lutBuffer != NULL)
				{
					*((uint32_t*)&buffer) = *((uint32_t*)currentSource);
					currentSource += 8;
					ind_lutd = LUT_INDEX(buffer[2], buffer[1], buffer[0]);
					*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
					currentDest += 3;
				}
				else
				{
					*currentDest++ = *currentSource--;
					*currentDest++ = *currentSource--;
					*currentDest++ = *currentSource;
					currentSource += 10;
				}
			}
		}
		return;
	}

	if (pixelFormat == PixelFormat::I420)
	{
		int deltaU = lineLength * height;
		int deltaV = lineLength * height * 5 / 4;
		for (int yDest = 0, ySource = 0; yDest < outputHeight; ySource += 2, ++yDest)
		{
			uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
			uint8_t* endDest = currentDest + destLineSize;
			uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource));
			uint8_t* currentSourceU = (uint8_t*)data + deltaU + (ySource * lineLength / 2) / 2;
			uint8_t* currentSourceV = (uint8_t*)data + deltaV + (ySource * lineLength / 2) / 2;

			while (currentDest < endDest)
			{
				*((uint8_t*)&buffer) = *((uint8_t*)currentSource);
				currentSource += 2;
				buffer[2] = *(currentSourceU++);
				buffer[3] = *(currentSourceV++);

				ind_lutd = LUT_INDEX(buffer[0], buffer[2], buffer[3]);

				*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
				currentDest += 3;
			}
		}
		return;
	}

	if (pixelFormat == PixelFormat::P010 && automaticToneMapping == nullptr)
	{
		uint16_t p010[2] = {};

		if (!FrameDecoderUtils::initialized)
		{
			initP010();
		}

		uint8_t* deltaUV = (dataUV != nullptr) ? (uint8_t*)dataUV : (uint8_t*)data + lineLength * height;
		for (int yDest = 0, ySource = 0; yDest < outputHeight; ySource += 2, ++yDest)
		{
			uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
			uint8_t* endDest = currentDest + destLineSize;
			uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource));
			uint8_t* currentSourceU = deltaUV + (((uint64_t)ySource / 2) * lineLength);

			while (currentDest < endDest)
			{
				memcpy(((uint16_t*)&p010), ((uint16_t*)currentSource), 2);
				if (toneMapping)
				{
					buffer[0] = lutP010_y[p010[0] >> 6];
				}
				else
				{
					buffer[0] = p010[0] >> 8;
				}
				currentSource += 4;
				memcpy(((uint32_t*)&p010), ((uint32_t*)currentSourceU), 4);
				if (toneMapping)
				{
					buffer[2] = lutP010_uv[p010[0] >> 6];
					buffer[3] = lutP010_uv[p010[1] >> 6];
				}
				else
				{
					buffer[2] = p010[0] >> 8;
					buffer[3] = p010[1] >> 8;
				}

				currentSourceU += 4;

				ind_lutd = LUT_INDEX(buffer[0], buffer[2], buffer[3]);

				*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
				currentDest += 3;
			}
		}
		return;
	}
	if (pixelFormat == PixelFormat::P010 && automaticToneMapping != nullptr)
	{
		uint16_t p010[2] = {};

		if (!FrameDecoderUtils::initialized)
		{
			initP010();
		}

		uint8_t* deltaUV = (dataUV != nullptr) ? (uint8_t*)dataUV : (uint8_t*)data + lineLength * height;
		for (int yDest = 0, ySource = 0; yDest < outputHeight; ySource += 2, ++yDest)
		{
			uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
			uint8_t* endDest = currentDest + destLineSize;
			uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource));
			uint8_t* currentSourceU = deltaUV + (((uint64_t)ySource / 2) * lineLength);

			while (currentDest < endDest)
			{
				memcpy(((uint16_t*)&p010), ((uint16_t*)currentSource), 2);
				if (toneMapping)
				{
					automaticToneMapping->checkY(p010[0] >> 8);

					buffer[0] = lutP010_y[p010[0] >> 6];
				}
				else
				{
					buffer[0] = automaticToneMapping->checkY(p010[0] >> 8);
				}
				currentSource += 4;
				memcpy(((uint32_t*)&p010), ((uint32_t*)currentSourceU), 4);
				if (toneMapping)
				{
					automaticToneMapping->checkU(p010[0] >> 8);
					automaticToneMapping->checkV(p010[1] >> 8);

					buffer[2] = lutP010_uv[p010[0] >> 6];
					buffer[3] = lutP010_uv[p010[1] >> 6];
				}
				else
				{
					buffer[2] = automaticToneMapping->checkU(p010[0] >> 8);
					buffer[3] = automaticToneMapping->checkV(p010[1] >> 8);
				}

				currentSourceU += 4;

				ind_lutd = LUT_INDEX(buffer[0], buffer[2], buffer[3]);

				*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
				currentDest += 3;
			}
		}
		automaticToneMapping->finilize();
		return;
	}

	if (pixelFormat == PixelFormat::NV12 && automaticToneMapping == nullptr)
	{
		uint8_t* deltaUV = (dataUV != nullptr) ? (uint8_t*)dataUV : (uint8_t*)data + lineLength * height;
		for (int yDest = 0, ySource = 0; yDest < outputHeight; ySource += 2, ++yDest)
		{
			uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
			uint8_t* endDest = currentDest + destLineSize;
			uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource));
			uint8_t* currentSourceU = deltaUV + (((uint64_t)ySource / 2) * lineLength);

			while (currentDest < endDest)
			{
				*((uint8_t*)&buffer) = *((uint8_t*)currentSource);
				currentSource += 2;
				*((uint16_t*)&(buffer[2])) = *((uint16_t*)currentSourceU);
				currentSourceU += 2;

				ind_lutd = LUT_INDEX(buffer[0], buffer[2], buffer[3]);

				*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
				currentDest += 3;
			}
		}
		return;
	}
	if (pixelFormat == PixelFormat::NV12 && automaticToneMapping != nullptr)
	{
		uint8_t* deltaUV = (dataUV != nullptr) ? (uint8_t*)dataUV : (uint8_t*)data + lineLength * height;
		for (int yDest = 0, ySource = 0; yDest < outputHeight; ySource += 2, ++yDest)
		{
			uint8_t* currentDest = destMemory + ((uint64_t)destLineSize) * yDest;
			uint8_t* endDest = currentDest + destLineSize;
			uint8_t* currentSource = (uint8_t*)data + (((uint64_t)lineLength * ySource));
			uint8_t* currentSourceU = deltaUV + (((uint64_t)ySource / 2) * lineLength);

			while (currentDest < endDest)
			{
				*((uint8_t*)&buffer) = *((uint8_t*)currentSource);
				currentSource += 2;
				*((uint16_t*)&(buffer[2])) = *((uint16_t*)currentSourceU);
				currentSourceU += 2;

				ind_lutd = LUT_INDEX((automaticToneMapping->checkY(buffer[0])), (automaticToneMapping->checkU(buffer[2])), (automaticToneMapping->checkV(buffer[3])));

				*((uint32_t*)currentDest) = *((uint32_t*)(&lutBuffer[ind_lutd]));
				currentDest += 3;
			}
		}
		automaticToneMapping->finilize();
		return;
	}
}





void FrameDecoder::applyLUT(uint8_t* _source, unsigned int width, unsigned int height, const uint8_t* lutBuffer, const int _hdrToneMappingEnabled)
{
	uint8_t buffer[8];

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
				{
					*((uint32_t*)&buffer) = *((uint32_t*)startSource);
					uint32_t ind_lutd = LUT_INDEX(buffer[0], buffer[1], buffer[2]);
					memcpy(startSource, &(lutBuffer[ind_lutd]), 3);
					startSource += 3;
				}
			}
			else
			{
				unsigned int x = 0;
				while (startSource < endSource)
				{
					if (x++ == sizeX)
						startSource += (width - 2 * static_cast<size_t>(sizeX)) * 3;

					*((uint32_t*)&buffer) = *((uint32_t*)startSource);
					uint32_t ind_lutd = LUT_INDEX(buffer[0], buffer[1], buffer[2]);
					memcpy(startSource, &(lutBuffer[ind_lutd]), 3);
					startSource += 3;
				}
			}
		}
	}
}

void FrameDecoder::processSystemImageBGRA(Image<ColorRgb>& image, int targetSizeX, int targetSizeY,
	int startX, int startY,
	uint8_t* source, int _actualWidth, int _actualHeight,
	int division, uint8_t* _lutBuffer, int lineSize)
{
	uint32_t	ind_lutd;
	uint8_t		buffer[8];
	size_t		divisionX = (size_t)division * 4;

	if (lineSize == 0)
		lineSize = _actualWidth * 4;

	for (int j = 0; j < targetSizeY; j++)
	{
		size_t lineSource = std::min(startY + j * division, _actualHeight - 1);
		uint8_t* dLine = (image.rawMem() + (size_t)j * targetSizeX * 3);
		uint8_t* dLineEnd = dLine + (size_t)targetSizeX * 3;
		uint8_t* sLine = ((source + (lineSource * lineSize) + ((size_t)startX * 4)));

		if (_lutBuffer == nullptr)
		{
			sLine += 2;
			while (dLine < dLineEnd)
			{
				*dLine++ = *sLine--;
				*dLine++ = *sLine--;
				*dLine++ = *sLine;
				sLine += divisionX + 2;
			}
		}
		else while (dLine < dLineEnd)
		{
			*((uint32_t*)&buffer) = *((uint32_t*)sLine);
			sLine += divisionX;
			ind_lutd = LUT_INDEX(buffer[2], buffer[1], buffer[0]);
			*((uint32_t*)dLine) = *((uint32_t*)(&_lutBuffer[ind_lutd]));
			dLine += 3;
		}
	}
}

void FrameDecoder::processSystemImageBGR(Image<ColorRgb>& image, int targetSizeX, int targetSizeY,
	int startX, int startY,
	uint8_t* source, int _actualWidth, int _actualHeight,
	int division, uint8_t* _lutBuffer, int lineSize)
{
	uint32_t	ind_lutd;
	uint8_t		buffer[8];
	size_t		divisionX = (size_t)division * 3;

	if (lineSize == 0)
		lineSize = _actualWidth * 3;

	for (int j = 0; j < targetSizeY; j++)
	{
		size_t lineSource = std::min(startY + j * division, _actualHeight - 1);
		uint8_t* dLine = (image.rawMem() + (size_t)j * targetSizeX * 3);
		uint8_t* dLineEnd = dLine + (size_t)targetSizeX * 3;
		uint8_t* sLine = ((source + (lineSource * lineSize) + ((size_t)startX * 3)));

		if (_lutBuffer == nullptr)
		{
			sLine += 2;
			while (dLine < dLineEnd)
			{
				*dLine++ = *sLine--;
				*dLine++ = *sLine--;
				*dLine++ = *sLine;
				sLine += divisionX + 2;
			}
		}
		else while (dLine < dLineEnd)
		{
			memcpy(&buffer, &sLine, 3);
			sLine += divisionX;
			ind_lutd = LUT_INDEX(buffer[2], buffer[1], buffer[0]);
			*((uint32_t*)dLine) = *((uint32_t*)(&_lutBuffer[ind_lutd]));
			dLine += 3;
		}
	}
}


void FrameDecoder::processSystemImageBGR16(Image<ColorRgb>& image, int targetSizeX, int targetSizeY,
	int startX, int startY,
	uint8_t* source, int _actualWidth, int _actualHeight,
	int division, uint8_t* _lutBuffer, int lineSize)
{
	uint32_t	ind_lutd;
	uint8_t		buffer[8];
	size_t		divisionX = (size_t)division * 2;

	if (lineSize == 0)
		lineSize = _actualWidth * 2;

	for (int j = 0; j < targetSizeY; j++)
	{
		size_t lineSource = std::min(startY + j * division, _actualHeight - 1);
		uint8_t* dLine = (image.rawMem() + (size_t)j * targetSizeX * 3);
		uint8_t* dLineEnd = dLine + (size_t)targetSizeX * 3;
		uint8_t* sLine = ((source + (lineSource * lineSize) + ((size_t)startX * 2)));

		if (_lutBuffer == nullptr)
		{
			sLine += 2;
			while (dLine < dLineEnd)
			{
				*((uint16_t*)&buffer) = *((uint16_t*)sLine);

				*dLine++ = (buffer[1] & 0xF8);
				*dLine++ = (((buffer[1] & 0x7) << 3) | (buffer[0] & 0xE0) >> 5) << 2;
				*dLine++ = (buffer[0] & 0x1f) << 3;

				sLine += divisionX;
			}
		}
		else while (dLine < dLineEnd)
		{
			*((uint16_t*)&buffer) = *((uint16_t*)sLine);
			sLine += divisionX;

			buffer[3] = (buffer[1] & 0xF8);
			buffer[4] = (((buffer[1] & 0x7) << 3) | (buffer[0] & 0xE0) >> 5) << 2;
			buffer[5] = (buffer[0] & 0x1f) << 3;

			ind_lutd = LUT_INDEX(buffer[3], buffer[4], buffer[5]);
			*((uint32_t*)dLine) = *((uint32_t*)(&_lutBuffer[ind_lutd]));
			dLine += 3;
		}
	}
}



void FrameDecoder::processSystemImageRGBA(Image<ColorRgb>& image, int targetSizeX, int targetSizeY,
											int startX, int startY,
											uint8_t* source, int _actualWidth, int _actualHeight,
											int division, uint8_t* _lutBuffer, int lineSize)
{
	uint32_t	ind_lutd;
	uint8_t		buffer[8];
	size_t		divisionX = (size_t)division * 4;

	if (lineSize == 0)
		lineSize = _actualWidth * 4;

	for (int j = 0; j < targetSizeY; j++)
	{
		size_t lineSource = std::min(startY + j * division, _actualHeight - 1);
		uint8_t* dLine = (image.rawMem() + (size_t)j * targetSizeX * 3);
		uint8_t* dLineEnd = dLine + (size_t)targetSizeX * 3;
		uint8_t* sLine = ((source + (lineSource * lineSize) + ((size_t)startX * 4)));

		if (_lutBuffer == nullptr)
		{
			while (dLine < dLineEnd)
			{
				*dLine++ = *sLine++;
				*dLine++ = *sLine++;
				*dLine++ = *sLine;
				sLine += divisionX - 2;
			}
		}
		else while (dLine < dLineEnd)
		{
			*((uint32_t*)&buffer) = *((uint32_t*)sLine);
			sLine += divisionX;
			ind_lutd = LUT_INDEX(buffer[2], buffer[1], buffer[0]);
			*((uint32_t*)dLine) = *((uint32_t*)(&_lutBuffer[ind_lutd]));
			dLine += 3;
		}
	}
}

void FrameDecoder::processSystemImagePQ10(Image<ColorRgb>& image, int targetSizeX, int targetSizeY,
	int startX, int startY,
	uint8_t* source, int _actualWidth, int _actualHeight,
	int division, uint8_t* _lutBuffer, int lineSize)
{
	uint32_t	ind_lutd;
	size_t		divisionX = (size_t)division * 4;

	if (lineSize == 0)
		lineSize = _actualWidth * 4;

	for (int j = 0; j < targetSizeY; j++)
	{
		size_t lineSource = std::min(startY + j * division, _actualHeight - 1);
		uint8_t* dLine = (image.rawMem() + (size_t)j * targetSizeX * 3);
		uint8_t* dLineEnd = dLine + (size_t)targetSizeX * 3;
		uint8_t* sLine = ((source + (lineSource * lineSize) + ((size_t)startX * 4)));

		if (_lutBuffer == nullptr)
		{
			while (dLine < dLineEnd)
			{
				uint32_t inS = *((uint32_t*)sLine);
				*(dLine++) = (inS >> 2) & 0xFF;
				*(dLine++) = (inS >> 12) & 0xFF;
				*(dLine++) = (inS >> 22) & 0xFF;				
				sLine += divisionX;
			}
		}
		else while (dLine < dLineEnd)
		{
			uint32_t inS = *((uint32_t*)sLine);			
			ind_lutd = LUT_INDEX(((inS >> 2) & 0xFF), ((inS >> 12) & 0xFF), ((inS >> 22) & 0xFF));
			*((uint32_t*)dLine) = *((uint32_t*)(&_lutBuffer[ind_lutd]));
			sLine += divisionX;
			dLine += 3;
		}
	}
}


