#include <base/ImageToLedsMap.h>
#include <base/ImageProcessor.h>

#define push_back_index(list, index) list.push_back((index) * 3)

using namespace hyperhdr;

ImageToLedsMap::ImageToLedsMap(
				Logger* _log,
				const int mappingType,
				const bool sparseProcessing,
				const unsigned width,
				const unsigned height,
				const unsigned horizontalBorder,
				const unsigned verticalBorder,
				const quint8 instanceIndex,
				const std::vector<Led>& leds)
	: _width(width)
	, _height(height)
	, _horizontalBorder(horizontalBorder)
	, _verticalBorder(verticalBorder)
	, _colorsMap()
	, _colorsDisabled()
	, _colorsGroups()
	, _groupMin(-1)
	, _groupMax(-1)
	, _haveDisabled(false)
{
	// Sanity check of the size of the borders (and width and height)
	Q_ASSERT(_width > 2 * _verticalBorder);
	Q_ASSERT(_height > 2 * _horizontalBorder);
	Q_ASSERT(_width < 10000);
	Q_ASSERT(_height < 10000);

	_mappingType = mappingType;

	// Reserve enough space in the map for the leds
	_colorsMap.reserve(leds.size());

	const int32_t xOffset = _verticalBorder;
	const int32_t actualWidth = _width - 2 * _verticalBorder;
	const int32_t yOffset = _horizontalBorder;
	const int32_t actualHeight = _height - 2 * _horizontalBorder;

	size_t   totalCount = 0;
	size_t   totalCapasity = 0;
	int      ledCounter = 0;

	for (const Led& led : leds)
	{
		ledCounter++;

		// skip leds without area
		if ((led.maxX_frac - led.minX_frac) < 1e-6 || (led.maxY_frac - led.minY_frac) < 1e-6)
		{
			_colorsMap.emplace_back();
			continue;
		}

		// Compute the index boundaries for this led
		int32_t minX_idx = xOffset + int32_t(qRound(actualWidth * led.minX_frac));
		int32_t maxX_idx = xOffset + int32_t(qRound(actualWidth * led.maxX_frac));
		int32_t minY_idx = yOffset + int32_t(qRound(actualHeight * led.minY_frac));
		int32_t maxY_idx = yOffset + int32_t(qRound(actualHeight * led.maxY_frac));

		// make sure that the area is at least a single led large
		minX_idx = qMin(minX_idx, xOffset + actualWidth - 1);
		if (minX_idx == maxX_idx)
		{
			maxX_idx++;
		}
		minY_idx = qMin(minY_idx, yOffset + actualHeight - 1);
		if (minY_idx == maxY_idx)
		{
			maxY_idx++;
		}

		// Add all the indices in the above defined rectangle to the indices for this led
		const int32_t maxYLedCount = qMin(maxY_idx, yOffset + actualHeight);
		const int32_t maxXLedCount = qMin(maxX_idx, xOffset + actualWidth);
		const int32_t realYLedCount = qAbs(maxYLedCount - minY_idx);
		const int32_t realXLedCount = qAbs(maxXLedCount - minX_idx);

		bool   sparseIndexes = sparseProcessing;
		size_t totalSize = realYLedCount* realXLedCount;

		if (!sparseIndexes && totalSize > 1600)
		{
			Warning(_log, "This is large image area for lamp: %i. It contains %i indexes for captured video frame so reduce it by four. Enabling 'sparse processing' option for you. Consider to enable it permanently in the processing configuration to hide that warning.", ledCounter, totalSize);
			sparseIndexes = true;
		}

		const int32_t increment = (sparseIndexes) ? 2 : 1;

		std::vector<int32_t> ledColor;
		ledColor.reserve( (sparseIndexes) ? ((realYLedCount / 2) + (realYLedCount % 2)) * ((realXLedCount / 2) + (realXLedCount % 2)) : totalSize);

		if (ImageProcessor::mappingTypeToInt(QString("weighted")) == _mappingType)
		{
			bool left = led.minX_frac == 0;
			bool right = led.maxX_frac == 1;
			bool top = led.minY_frac == 0;
			bool bottom = led.maxY_frac == 1;

			int isCorner = ((left) ? 1 : 0) + ((right) ? 1 : 0) + ((top) ? 1 : 0) + ((bottom) ? 1 : 0);

			if (isCorner != 1)
			{
				for (int32_t y = minY_idx; y < maxYLedCount; y += increment)
				{
					for (int32_t x = minX_idx; x < maxXLedCount; x += increment)
					{
						push_back_index(ledColor, y * width + x);
					}
				}
			}
			else if (bottom)
			{
				int32_t mid = (minY_idx + maxYLedCount) / 2;
				for (int32_t y = minY_idx; y < mid; y += increment)
					for (int32_t x = minX_idx; x < maxXLedCount; x += increment)
						push_back_index(ledColor, -((int32_t)(y * width + x)));

				for (int32_t y = mid; y < maxYLedCount; y += increment)
					for (int32_t x = minX_idx; x < maxXLedCount; x += increment)
						push_back_index(ledColor, y * width + x);
			}
			else if (top)
			{
				int32_t mid = (minY_idx + maxYLedCount) / 2;
				for (int32_t y = minY_idx; y < mid; y += increment)
					for (int32_t x = minX_idx; x < maxXLedCount; x += increment)
						push_back_index(ledColor, y * width + x);

				for (int32_t y = mid; y < maxYLedCount; y += increment)
					for (int32_t x = minX_idx; x < maxXLedCount; x += increment)
						push_back_index(ledColor, -((int32_t)(y * width + x)));
			}

			else if (left)
			{
				int32_t mid = (minX_idx + maxXLedCount) / 2;
				for (int32_t y = minY_idx; y < maxYLedCount; y += increment)
				{
					for (int32_t x = minX_idx; x < mid; x += increment)
						push_back_index(ledColor, y * width + x);
					for (int32_t x = mid; x < maxXLedCount; x += increment)
						push_back_index(ledColor, -((int32_t)(y * width + x)));
				}
			}

			else if (right)
			{
				int32_t mid = (minX_idx + maxXLedCount) / 2;
				for (int32_t y = minY_idx; y < maxYLedCount; y += increment)
				{
					for (int32_t x = minX_idx; x < mid; x += increment)
						push_back_index(ledColor, -((int32_t)(y * width + x)));
					for (int32_t x = mid; x < maxXLedCount; x += increment)
						push_back_index(ledColor, y * width + x);
				}
			}

		}
		else
		{
			for (int32_t y = minY_idx; y < maxYLedCount; y += increment)
			{
				for (int32_t x = minX_idx; x < maxXLedCount; x += increment)
				{
					push_back_index(ledColor, y * width + x);
				}
			}
		}

		// Add the constructed vector to the map
		_haveDisabled |= led.disabled;
		_colorsMap.push_back(ledColor);
		_colorsDisabled.push_back(led.disabled);
		_colorsGroups.push_back(led.group);
		if (_groupMin == -1 || led.group < _groupMin)
			_groupMin = led.group;
		if (_groupMax == -1 || led.group > _groupMax)
			_groupMax = led.group;
		
		totalCount += ledColor.size();		
		totalCapasity += ledColor.capacity();
	}
	Info(_log, "Total index number is: %d (memory: %d). User sparse processing is: %s, image size: %d x %d, area number: %d",
		totalCount, totalCapasity, (sparseProcessing) ? "enabled" : "disabled", width, height, leds.size());
}

unsigned ImageToLedsMap::width() const
{
	return _width;
}

unsigned ImageToLedsMap::height() const
{
	return _height;
}


unsigned ImageToLedsMap::horizontalBorder() const
{
	return _horizontalBorder;
}

unsigned ImageToLedsMap::verticalBorder() const {
	return _verticalBorder;
}

std::vector<ColorRgb> ImageToLedsMap::Process(const Image<ColorRgb>& image, uint16_t* advanced)
{
	std::vector<ColorRgb> colors;
	switch (_mappingType)
	{
		case 3:
		case 2: colors = getMeanAdvLedColor(image, advanced); break;
		case 1: colors = getUniLedColor(image); break;
		default: colors = getMeanLedColor(image);
	}
	
	if (_groupMax > 0 && _mappingType != 1)
	{
		for (int i = std::max(_groupMin, 1); i <= _groupMax; i++)
		{
			int32_t r = 0, g = 0, b = 0, c = 0;

			auto groupIn = _colorsGroups.begin();
			for (auto _rgb = colors.begin(); _rgb != colors.end(); _rgb++, groupIn++)
				if (*groupIn == i)
				{
					r += (*_rgb).red;
					g += (*_rgb).green;
					b += (*_rgb).blue;
					c++;
				}

			if (c > 0)
			{
				r /= c;
				g /= c;
				b /= c;
			}

			auto groupOut = _colorsGroups.begin();
			for (auto _rgb = colors.begin(); _rgb != colors.end(); _rgb++, groupOut++)
				if (*groupOut == i)
				{
					(*_rgb).red = r;
					(*_rgb).green = g;
					(*_rgb).blue = b;
				}
		}
	}

	for (size_t i = 0; _haveDisabled && i < qMin(_colorsDisabled.size(), colors.size()); i++)
		if (_colorsDisabled[i])
		{
			colors[i].red = 0;
			colors[i].green = 0;
			colors[i].blue = 0;
		}

	return colors;
}

std::vector<ColorRgb> ImageToLedsMap::getMeanLedColor(const Image<ColorRgb>& image) const
{
	std::vector<ColorRgb> ledColors(_colorsMap.size(), ColorRgb{ 0,0,0 });

	// Sanity check for the number of leds
	//assert(_colorsMap.size() == ledColors.size());
	if (_colorsMap.size() != ledColors.size())
	{
		Debug(Logger::getInstance("HYPERHDR"), "ImageToLedsMap: colorsMap.size != ledColors.size -> %d != %d", _colorsMap.size(), ledColors.size());
		return ledColors;
	}

	// Iterate each led and compute the mean
	auto led = ledColors.begin();
	for (auto colors = _colorsMap.begin(); colors != _colorsMap.end(); ++colors, ++led)
	{
		const ColorRgb color = calcMeanColor(image, *colors);
		*led = color;
	}

	return ledColors;
}

std::vector<ColorRgb> ImageToLedsMap::getUniLedColor(const Image<ColorRgb>& image) const
{
	std::vector<ColorRgb> ledColors(_colorsMap.size(), ColorRgb{ 0,0,0 });

	// Sanity check for the number of leds
	// assert(_colorsMap.size() == ledColors.size());
	if (_colorsMap.size() != ledColors.size())
	{
		Debug(Logger::getInstance("HYPERHDR"), "ImageToLedsMap: colorsMap.size != ledColors.size -> %d != %d", _colorsMap.size(), ledColors.size());
		return ledColors;
	}

	// calculate uni color
	const ColorRgb color = calcMeanColor(image);
	std::fill(ledColors.begin(), ledColors.end(), color);

	return ledColors;
}


std::vector<ColorRgb> ImageToLedsMap::getMeanAdvLedColor(const Image<ColorRgb>& image, uint16_t* lut) const
{
	std::vector<ColorRgb> ledColors(_colorsMap.size(), ColorRgb{ 0,0,0 });

	// Sanity check for the number of leds
	//assert(_colorsMap.size() == ledColors.size());
	if (_colorsMap.size() != ledColors.size())
	{
		Debug(Logger::getInstance("HYPERHDR"), "ImageToLedsMap: colorsMap.size != ledColors.size -> %d != %d", _colorsMap.size(), ledColors.size());
		return ledColors;
	}

	// Iterate each led and compute the mean
	auto led = ledColors.begin();
	for (auto colors = _colorsMap.begin(); colors != _colorsMap.end(); ++colors, ++led)
	{
		const ColorRgb color = calcMeanAdvColor(image, *colors, lut);
		*led = color;
	}

	return ledColors;
}

ColorRgb ImageToLedsMap::calcMeanColor(const Image<ColorRgb>& image, const std::vector<int32_t>& colors) const
{
	const auto colorVecSize = colors.size();

	if (colorVecSize == 0)
	{
		return ColorRgb::BLACK;
	}

	// Accumulate the sum of each separate color channel
	uint_fast32_t sumRed = 0;
	uint_fast32_t sumGreen = 0;
	uint_fast32_t sumBlue = 0;
	const uint8_t* imgData = image.rawMem();

	for (const unsigned colorOffset : colors)
	{
		sumRed += imgData[colorOffset];
		sumGreen += imgData[colorOffset + 1];
		sumBlue += imgData[colorOffset + 2];
	}

	// Compute the average of each color channel
	const uint8_t avgRed = uint8_t(sumRed / colorVecSize);
	const uint8_t avgGreen = uint8_t(sumGreen / colorVecSize);
	const uint8_t avgBlue = uint8_t(sumBlue / colorVecSize);

	// Return the computed color
	return { avgRed, avgGreen, avgBlue };
}

ColorRgb ImageToLedsMap::calcMeanAdvColor(const Image<ColorRgb>& image, const std::vector<int32_t>& colors, uint16_t* lut) const
{
	const auto colorVecSize = colors.size();

	if (colorVecSize == 0)
	{
		return ColorRgb::BLACK;
	}

	// Accumulate the sum of each seperate color channel
	uint_fast64_t sum1 = 0;
	uint_fast64_t sumRed1 = 0;
	uint_fast64_t sumGreen1 = 0;
	uint_fast64_t sumBlue1 = 0;

	uint_fast64_t sum2 = 0;
	uint_fast64_t sumRed2 = 0;
	uint_fast64_t sumGreen2 = 0;
	uint_fast64_t sumBlue2 = 0;

	const uint8_t* imgData = image.rawMem();

	for (const int32_t colorOffset : colors)
	{
		if (colorOffset >= 0) {
			sumRed1 += lut[imgData[colorOffset]];
			sumGreen1 += lut[imgData[colorOffset + 1]];
			sumBlue1 += lut[imgData[colorOffset + 2]];
			sum1++;
		}
		else {
			sumRed2 += lut[imgData[(-colorOffset)]];
			sumGreen2 += lut[imgData[(-colorOffset) + 1]];
			sumBlue2 += lut[imgData[(-colorOffset) + 2]];
			sum2++;
		}
	}


	if (sum1 > 0 && sum2 > 0)
	{
		uint16_t avgRed = std::min((uint32_t)sqrt(((sumRed1 * 3) / sum1 + sumRed2 / sum2) / 4), (uint32_t)255);
		uint16_t avgGreen = std::min((uint32_t)sqrt(((sumGreen1 * 3) / sum1 + sumGreen2 / sum2) / 4), (uint32_t)255);
		uint16_t avgBlue = std::min((uint32_t)sqrt(((sumBlue1 * 3) / sum1 + sumBlue2 / sum2) / 4), (uint32_t)255);

		return { (uint8_t)avgRed, (uint8_t)avgGreen, (uint8_t)avgBlue };
	}
	else
	{
		uint16_t avgRed = std::min((uint32_t)sqrt((sumRed1 + sumRed2) / (sum1 + sum2)), (uint32_t)255);
		uint16_t avgGreen = std::min((uint32_t)sqrt((sumGreen1 + sumGreen2) / (sum1 + sum2)), (uint32_t)255);
		uint16_t avgBlue = std::min((uint32_t)sqrt((sumBlue1 + sumBlue2) / (sum1 + sum2)), (uint32_t)255);

		return { (uint8_t)avgRed, (uint8_t)avgGreen, (uint8_t)avgBlue };
	}
}

ColorRgb ImageToLedsMap::calcMeanColor(const Image<ColorRgb>& image) const
{
	// Accumulate the sum of each separate color channel
	uint_fast32_t sumRed = 0;
	uint_fast32_t sumGreen = 0;
	uint_fast32_t sumBlue = 0;
	const size_t imageSize = image.size();

	const uint8_t* imgData = image.rawMem();

	for (size_t idx = 0; idx < imageSize; idx += 3)
	{
		sumRed += imgData[idx];
		sumGreen += imgData[idx + 1];
		sumBlue += imgData[idx + 2];
	}

	// Compute the average of each color channel
	const uint8_t avgRed = uint8_t(sumRed / imageSize);
	const uint8_t avgGreen = uint8_t(sumGreen / imageSize);
	const uint8_t avgBlue = uint8_t(sumBlue / imageSize);

	// Return the computed color
	return { avgRed, avgGreen, avgBlue };
}
