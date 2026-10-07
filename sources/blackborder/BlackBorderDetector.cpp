// BlackBorders includes
#include <blackborder/BlackBorderDetector.h>
#include <algorithm>
#include <array>
#include <cmath>

using namespace hyperhdr;

BlackBorderDetector::BlackBorderDetector(double threshold)
	: _blackborderThreshold(calculateThreshold(threshold))
{
}

uint8_t BlackBorderDetector::calculateThreshold(double threshold)
{
	int rgbThreshold = int(std::ceil(threshold * 255));

	if (rgbThreshold < 0)
		rgbThreshold = 0;
	else if (rgbThreshold > 255)
		rgbThreshold = 255;

	uint8_t blackborderThreshold = uint8_t(rgbThreshold);

	return blackborderThreshold;
}

bool BlackBorder::operator== (const BlackBorder& other) const
{
	if (unknown)
	{
		return other.unknown;
	}

	return (other.unknown == false) && (horizontalSize == other.horizontalSize) && (verticalSize == other.verticalSize);
}

BlackBorder BlackBorderDetector::process(const Image<ColorRgb>& image) const
{
	// test centre and 33%, 66% of width/height
	// 33 and 66 will check left and top
	// centre will check right and bottom sides
	int width = image.width();
	int height = image.height();
	int width33percent = width / 3;
	int height33percent = height / 3;
	int width66percent = width33percent * 2;
	int height66percent = height33percent * 2;
	int xCenter = width / 2;
	int yCenter = height / 2;


	int firstNonBlackXPixelIndex = -1;
	int firstNonBlackYPixelIndex = -1;

	width--; // remove 1 pixel to get end pixel index
	height--;

	// find first X pixel of the image
	for (int x = 0; x < width33percent; ++x)
	{
		if (!isBlack(image((width - x), yCenter))
			|| !isBlack(image(x, height33percent))
			|| !isBlack(image(x, height66percent)))
		{
			firstNonBlackXPixelIndex = x;
			break;
		}
	}

	// find first Y pixel of the image
	for (int y = 0; y < height33percent; ++y)
	{
		if (!isBlack(image(xCenter, (height - y)))
			|| !isBlack(image(width33percent, y))
			|| !isBlack(image(width66percent, y)))
		{
			firstNonBlackYPixelIndex = y;
			break;
		}
	}

	// Construct result
	BlackBorder detectedBorder{};

	detectedBorder.unknown = firstNonBlackXPixelIndex == -1 || firstNonBlackYPixelIndex == -1;
	detectedBorder.horizontalSize = firstNonBlackYPixelIndex;
	detectedBorder.verticalSize = firstNonBlackXPixelIndex;

	return detectedBorder;
}


///
/// classic detection mode (topleft single line mode)
BlackBorder BlackBorderDetector::process_classic(const Image<ColorRgb>& image) const
{
	// only test the topleft third of the image
	int width = image.width() / 3;
	int height = image.height() / 3;
	int maxSize = std::max(width, height);

	int firstNonBlackXPixelIndex = -1;
	int firstNonBlackYPixelIndex = -1;

	// find some pixel of the image
	for (int i = 0; i < maxSize; ++i)
	{
		int x = std::min(i, width);
		int y = std::min(i, height);

		const ColorRgb& color = image(x, y);
		if (!isBlack(color))
		{
			firstNonBlackXPixelIndex = x;
			firstNonBlackYPixelIndex = y;
			break;
		}
	}

	// expand image to the left
	for (; firstNonBlackXPixelIndex > 0; --firstNonBlackXPixelIndex)
	{
		const ColorRgb& color = image(firstNonBlackXPixelIndex - 1, firstNonBlackYPixelIndex);
		if (isBlack(color))
		{
			break;
		}
	}

	// expand image to the top
	for (; firstNonBlackYPixelIndex > 0; --firstNonBlackYPixelIndex)
	{
		const ColorRgb& color = image(firstNonBlackXPixelIndex, firstNonBlackYPixelIndex - 1);
		if (isBlack(color))
		{
			break;
		}
	}

	// Construct result
	BlackBorder detectedBorder{};

	detectedBorder.unknown = firstNonBlackXPixelIndex == -1 || firstNonBlackYPixelIndex == -1;
	detectedBorder.horizontalSize = firstNonBlackYPixelIndex;
	detectedBorder.verticalSize = firstNonBlackXPixelIndex;

	return detectedBorder;
}


///
/// letterbox detection mode (5lines top-bottom only detection)
BlackBorder BlackBorderDetector::process_letterbox(const Image<ColorRgb>& image) const
{
	// test center and 25%, 75% of width
	// 25 and 75 will check both top and bottom
	// center will only check top (minimise false detection of captions)
	int width = image.width();
	int height = image.height();
	int width25percent = width / 4;
	int height33percent = height / 3;
	int width75percent = width25percent * 3;
	int xCenter = width / 2;


	int firstNonBlackYPixelIndex = -1;

	height--; // remove 1 pixel to get end pixel index

	// find first Y pixel of the image
	for (int y = 0; y < height33percent; ++y)
	{
		if (!isBlack(image(xCenter, y))
			|| !isBlack(image(width25percent, y))
			|| !isBlack(image(width75percent, y))
			|| !isBlack(image(width25percent, (height - y)))
			|| !isBlack(image(width75percent, (height - y))))
		{
			firstNonBlackYPixelIndex = y;
			break;
		}
	}

	// Construct result
	BlackBorder detectedBorder{};

	detectedBorder.unknown = firstNonBlackYPixelIndex == -1;
	detectedBorder.horizontalSize = firstNonBlackYPixelIndex;
	detectedBorder.verticalSize = 0;

	return detectedBorder;
}

// Accept scan positions from the 10% grid in issue #821. Invalid or empty
// selections fall back to defaults so detection always has samples on both sides.
void BlackBorderDetector::setSubtitleScanlines(const std::vector<int>& top, const std::vector<int>& bottom)
{
	auto toMask = [](const std::vector<int>& positions, uint16_t fallback) {
		uint16_t mask = 0;
		for (int position : positions)
		{
			if (position >= int(SCANLINE_SPACING) && position <= int(SCANLINE_COUNT * SCANLINE_SPACING) && position % SCANLINE_SPACING == 0)
				mask |= 1u << (position / SCANLINE_SPACING - 1);
		}
		return mask ? mask : fallback;
	};
	_subtitleTopScanlines = toMask(top, DEFAULT_TOP_SCANLINES);
	_subtitleBottomScanlines = toMask(bottom, DEFAULT_BOTTOM_SCANLINES);
}

// Configurable letterbox scans inspired by https://github.com/awawa-dev/HyperHDR/issues/821.
// Bottom samples default to 10% and 90% to avoid wide centered subtitles.
// The earliest non-black sample on either edge bounds the symmetric crop,
// preserving the smaller border when the top and bottom bars differ.
BlackBorder BlackBorderDetector::process_subtitle(const Image<ColorRgb>& image) const
{
	const BlackBorder unknownBorder{ true, -1, 0 };
	if (image.width() == 0 || image.height() < 3)
		return unknownBorder;

	std::array<unsigned, SCANLINE_COUNT> topPositions{};
	std::array<unsigned, SCANLINE_COUNT> bottomPositions{};
	unsigned topCount = 0;
	unsigned bottomCount = 0;
	for (unsigned i = 0; i < SCANLINE_COUNT; ++i)
	{
		// Map percentages onto valid pixel indices, including very narrow images.
		const unsigned x = uint64_t(image.width() - 1) * ((i + 1) * SCANLINE_SPACING) / 100;
		if (_subtitleTopScanlines & (1u << i))
			topPositions[topCount++] = x;
		if (_subtitleBottomScanlines & (1u << i))
			bottomPositions[bottomCount++] = x;
	}

	const unsigned lastRow = image.height() - 1;
	for (unsigned y = 0; y < image.height() / 3; ++y)
	{
		for (unsigned i = 0; i < topCount; ++i)
			if (!isBlack(image(topPositions[i], y)))
				return { false, int(y), 0 };
		for (unsigned i = 0; i < bottomCount; ++i)
			if (!isBlack(image(bottomPositions[i], lastRow - y)))
				return { false, int(y), 0 };
	}
	return unknownBorder;
}

///
/// osd detection mode (find x then y at detected x to avoid changes by osd overlays)
BlackBorder BlackBorderDetector::process_osd(const Image<ColorRgb>& image) const
{
	// find X position at height33 and height66 we check from the left side, Ycenter will check from right side
	// then we try to find a pixel at this X position from top and bottom and right side from top
	int width = image.width();
	int height = image.height();
	int width33percent = width / 3;
	int height33percent = height / 3;
	int height66percent = height33percent * 2;
	int yCenter = height / 2;


	int firstNonBlackXPixelIndex = -1;
	int firstNonBlackYPixelIndex = -1;

	width--; // remove 1 pixel to get end pixel index
	height--;

	// find first X pixel of the image
	int x;
	for (x = 0; x < width33percent; ++x)
	{
		if (!isBlack(image((width - x), yCenter))
			|| !isBlack(image(x, height33percent))
			|| !isBlack(image(x, height66percent)))
		{
			firstNonBlackXPixelIndex = x;
			break;
		}
	}

	// find first Y pixel of the image
	for (int y = 0; y < height33percent; ++y)
	{
		// left side top + left side bottom + right side top  +  right side bottom
		if (!isBlack(image(x, y))
			|| !isBlack(image(x, (height - y)))
			|| !isBlack(image((width - x), y))
			|| !isBlack(image((width - x), (height - y))))
		{
			//					std::cout << "y " << y << " lt " << int(isBlack(color1)) << " lb " << int(isBlack(color2)) << " rt " << int(isBlack(color3)) << " rb " << int(isBlack(color4)) << std::endl;
			firstNonBlackYPixelIndex = y;
			break;
		}
	}

	// Construct result
	BlackBorder detectedBorder{};
	detectedBorder.unknown = firstNonBlackXPixelIndex == -1 || firstNonBlackYPixelIndex == -1;
	detectedBorder.horizontalSize = firstNonBlackYPixelIndex;
	detectedBorder.verticalSize = firstNonBlackXPixelIndex;
	return detectedBorder;
}
