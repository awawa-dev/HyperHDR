#pragma once

#include <image/Image.h>

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
		uint8_t calculateThreshold(double blackborderThreshold) const;
		BlackBorder process(const Image<ColorRgb>& image) const;
		BlackBorder process_classic(const Image<ColorRgb>& image) const;
		BlackBorder process_osd(const Image<ColorRgb>& image) const;
		BlackBorder process_letterbox(const Image<ColorRgb>& image) const;

	private:
		inline bool isBlack(const ColorRgb& color) const
		{
			return (color.red < _blackborderThreshold) && (color.green < _blackborderThreshold) && (color.blue < _blackborderThreshold);
		}

	private:
		const uint8_t _blackborderThreshold;
	};
}
