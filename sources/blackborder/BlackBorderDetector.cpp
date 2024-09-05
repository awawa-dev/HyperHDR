#include <iostream>
#include <utils/Logger.h>

// BlackBorders includes
#include <blackborder/BlackBorderDetector.h>
#include <cmath>

using namespace hyperhdr;

BlackBorderDetector::BlackBorderDetector(double threshold)
	: _blackborderThreshold(calculateThreshold(threshold))
{
}

uint8_t BlackBorderDetector::calculateThreshold(double threshold) const
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
