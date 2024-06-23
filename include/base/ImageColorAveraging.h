#pragma once

#ifndef PCH_ENABLED
	#include <cassert>
	#include <sstream>
	#include <math.h>
	#include <algorithm>
#endif

#include <image/Image.h>
#include <utils/Logger.h>
#include <led-strip/LedString.h>

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

		void Process(std::vector<ColorRgb>& colors, const Image<ColorRgb>& image, uint16_t* advanced);

	private:
		void getMeanLedColor(std::vector<ColorRgb>& ledColors, const Image<ColorRgb>& image) const;
		void getUniLedColor(std::vector<ColorRgb>& ledColors, const Image<ColorRgb>& image) const;
		void getMeanAdvLedColor(std::vector<ColorRgb>& ledColors, const Image<ColorRgb>& image, uint16_t* lut) const;

		const unsigned _width;
		const unsigned _height;
		const unsigned _horizontalBorder;
		const unsigned _verticalBorder;
		int _mappingType;

		std::vector<std::vector<int32_t>> _colorsMap;
		std::vector<int> _colorsGroups;

		int _groupMin;
		int _groupMax;

		ColorRgb calcMeanColor(const Image<ColorRgb>& image, const std::vector<int32_t>& colors) const;
		ColorRgb calcMeanAdvColor(const Image<ColorRgb>& image, const std::vector<int32_t>& colors, uint16_t* lut) const;
		ColorRgb calcMeanColor(const Image<ColorRgb>& image) const;
	};
}
