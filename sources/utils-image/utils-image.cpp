/* utils-image.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2024 awawa-dev
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

#include <iostream>
#include <sstream>
#include <string>
#include <limits>
#include <cmath>
#include <algorithm>

#include <turbojpeg.h>
#include <lunasvg.h>
#ifdef __clang__
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
#include <stb_image_write.h>
#ifdef __clang__
	#pragma clang diagnostic pop
#endif
#include <stb_image.h>
#include <utils-image/utils-image.h>

#include <utility>	
#include <list>

namespace utils_image
{
	ColorRgb _IMAGE_SHARED_API colorRgbfromString(const std::string& colorName)
	{
		std::list<std::pair<std::string, ColorRgb>> colors = {
			{ "white", ColorRgb(255,255,255)},
			{ "red", ColorRgb(255,0,0)},
			{ "green",ColorRgb(0,255,0)},
			{ "blue", ColorRgb(0,0,255)},
			{ "yellow", ColorRgb(255,255,0)},
			{ "magenta", ColorRgb(255,0,255)},
			{ "cyan", ColorRgb(0,255,255)}
		};

		std::list<std::pair<std::string, ColorRgb>>::iterator findIter = std::find_if(colors.begin(), colors.end(),
			[&colorName](const std::pair<std::string, ColorRgb>& a)
			{
				if (a.first == colorName)
					return true;
				else
					return false;
			}
		);

		if (findIter != colors.end())
			return (*findIter).second;

		return ColorRgb(255, 255, 255);
	}

	void _IMAGE_SHARED_API svg2png(const std::string& svgFile, int width, int height, std::vector<uint8_t>& buffer)
	{
		const std::uint32_t bgColor = 0x00000000;
		
		auto document = lunasvg::Document::loadFromData(svgFile);

		auto bitmap = document->renderToBitmap(width, height, bgColor);
		if (!bitmap.valid())
			return;
		bitmap.convertToRGBA();

		int len;
		unsigned char* png = stbi_write_png_to_mem(bitmap.data(), 0, bitmap.width(), bitmap.height(), 4, &len);
		if (png == NULL)
			return;

		buffer.assign(png, png + len);

		STBIW_FREE(png);
	}

	Image<ColorRgb> _IMAGE_SHARED_API load2image(const uint8_t* buffer, size_t size)
	{
		Image<ColorRgb> ret;
		int w, h, comp;

		unsigned char* image = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(buffer), size, &w, &h, &comp, STBI_rgb);

		if (image != nullptr)
		{
			ret.resize(w, h);
			memcpy(ret.rawMem(), image, 3ll * w * h);
		}

		STBIW_FREE(image);

		return ret;
	}

	void _IMAGE_SHARED_API encodeJpeg(std::vector<uint8_t>& buffer, Image<ColorRgb>& inputImage, bool scaleDown)
	{
		const int aspect = (scaleDown) ? 2 : 1;
		const int width = inputImage.width();
		const int height = inputImage.height() / aspect;
		int pitch = width * sizeof(ColorRgb) * aspect;
		int subSample = (scaleDown) ? TJSAMP_422 : TJSAMP_444;
		int quality = 75;

		unsigned long compressedImageSize = 0;
		unsigned char* compressedImage = NULL;

		tjhandle _jpegCompressor = tjInitCompress();

		tjCompress2(_jpegCompressor, inputImage.rawMem(), width, pitch, height, TJPF_RGB,
			&compressedImage, &compressedImageSize, subSample, quality, TJFLAG_FASTDCT);

		buffer.resize(compressedImageSize);
		memcpy(buffer.data(), compressedImage, compressedImageSize);

		tjDestroy(_jpegCompressor);
		tjFree(compressedImage);
	}

	bool _IMAGE_SHARED_API savePng(const std::string& filename, const Image<ColorRgb>& image)
	{
		return stbi_write_png(filename.c_str(), image.width(), image.height(), 3, image.rawMem(), 0);
	}

	Image<ColorRgb> _IMAGE_SHARED_API load2image(const std::string& filename)
	{
		Image<ColorRgb> ret;
		int w, h, comp;		

		unsigned char* image = stbi_load(filename.c_str(), &w, &h, &comp, STBI_rgb);

		if (image != nullptr)
		{
			ret.resize(w, h);
			memcpy(ret.rawMem(), image, 3ll * w * h);
		}

		STBIW_FREE(image);

		return ret;
	}
};
