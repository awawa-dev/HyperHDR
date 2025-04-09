#pragma once

#ifndef PCH_ENABLED
	#include <cstring>
	#include <vector>
	#include <cstdint>
#endif

#include <image/ColorRgb.h>
#include <image/Image.h>

#ifndef _IMAGE_SHARED_API
	#define _IMAGE_SHARED_API
#endif

namespace utils_image
{
	Image<ColorRgb> _IMAGE_SHARED_API load2image(const uint8_t* buffer, size_t size);
	Image<ColorRgb> _IMAGE_SHARED_API load2image(const std::string& filename);
	void _IMAGE_SHARED_API svg2png(const std::string& svgFile, int width, int height, std::vector<uint8_t>& buffer);
	ColorRgb _IMAGE_SHARED_API colorRgbfromString(const std::string& colorName);
	void _IMAGE_SHARED_API encodeJpeg(std::vector<uint8_t>& buffer, Image<ColorRgb>& inputImage, bool scaleDown);
	bool _IMAGE_SHARED_API savePng(const std::string& filename, const Image<ColorRgb>& image);
};
