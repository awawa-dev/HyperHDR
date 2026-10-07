#pragma once

#include <image/Image.h>
#include <vector>

namespace hyperhdr
{
	struct BlackBorder
	{
		bool unknown;
		int horizontalSize;
		int verticalSize;
		bool operator== (const BlackBorder& other) const;
	};

	class BlackBorderDetector
	{
	public:
		BlackBorderDetector(double threshold);
		static uint8_t calculateThreshold(double blackborderThreshold);
		BlackBorder process(const Image<ColorRgb>& image) const;
		BlackBorder process_classic(const Image<ColorRgb>& image) const;
		BlackBorder process_osd(const Image<ColorRgb>& image) const;
		BlackBorder process_letterbox(const Image<ColorRgb>& image) const;
		BlackBorder process_subtitle(const Image<ColorRgb>& image) const;
		void setSubtitleScanlines(const std::vector<int>& top, const std::vector<int>& bottom);

	private:
		inline bool isBlack(const ColorRgb& color) const
		{
			return (color.red < _blackborderThreshold) && (color.green < _blackborderThreshold) && (color.blue < _blackborderThreshold);
		}

	private:
		static constexpr unsigned SCANLINE_COUNT = 9;
		static constexpr unsigned SCANLINE_SPACING = 10;
		static constexpr uint16_t DEFAULT_TOP_SCANLINES = (1u << SCANLINE_COUNT) - 1;
		static constexpr uint16_t DEFAULT_BOTTOM_SCANLINES = 1u | (1u << (SCANLINE_COUNT - 1));
		const uint8_t _blackborderThreshold;
		uint16_t _subtitleTopScanlines = DEFAULT_TOP_SCANLINES;
		uint16_t _subtitleBottomScanlines = DEFAULT_BOTTOM_SCANLINES;
	};
}
