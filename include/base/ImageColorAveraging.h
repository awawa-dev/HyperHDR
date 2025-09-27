#pragma once

#ifndef PCH_ENABLED
	#include <cassert>
	#include <sstream>
	#include <math.h>
	#include <algorithm>
	#include <vector>
	#include <map>
#endif

#include <image/Image.h>
#include <utils/Logger.h>
#include <base/LedString.h>

#include <linalg.h>

namespace hyperhdr
{	
	class ImageColorAveraging
	{
	public:
		ImageColorAveraging(
			Logger* _log,
			const int mappingType,
			const bool sparseProcessing,
			const unsigned width,
			const unsigned height,
			const unsigned horizontalBorder,
			const unsigned verticalBorder,
			const quint8 instanceIndex,
			const std::vector<LedString::Led>& leds);

		unsigned width() const;
		unsigned height() const;

		unsigned horizontalBorder() const;
		unsigned verticalBorder() const;

		void process(std::vector<linalg::aliases::float3>& ledColors, const Image<ColorRgb>& image);

	private:
		void getUnicolorForLeds(std::vector<linalg::aliases::float3>& ledColors, const Image<ColorRgb>& image) const;
		void getMulticolorForLeds(std::vector<linalg::aliases::float3>& ledColors, const Image<ColorRgb>& image) const;

		const unsigned _width;
		const unsigned _height;
		const bool	_sparseProcessing;
		const unsigned _horizontalBorder;
		const unsigned _verticalBorder;
		int _mappingType;

		std::vector<std::vector<uint32_t>> _colorsMap;
		std::map<int, std::vector<uint32_t>> _colorGroups;

		linalg::aliases::float3 calcMulticolorForLeds(const Image<ColorRgb>& image, const std::vector<uint32_t>& colors) const;
		linalg::aliases::float3 calcUnicolorForLeds(const Image<ColorRgb>& image) const;
	};
}
