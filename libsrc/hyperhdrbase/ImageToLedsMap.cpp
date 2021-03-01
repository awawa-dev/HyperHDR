#include <hyperhdrbase/ImageToLedsMap.h>
#include <hyperhdrbase/ImageProcessor.h>

using namespace hyperhdr;

ImageToLedsMap::ImageToLedsMap(
		Logger* _log,
		int mappingType,
		bool sparseProcessing,
		unsigned width,
		unsigned height,
		unsigned horizontalBorder,
		unsigned verticalBorder,
		quint8 instanceIndex,
		const std::vector<Led>& leds)
	: _width(width)
	, _height(height)
	, _sparseProcessing(sparseProcessing)
	, _horizontalBorder(horizontalBorder)
	, _verticalBorder(verticalBorder)
	, _instanceIndex(instanceIndex)
	, _colorsMap()
{
	// Sanity check of the size of the borders (and width and height)
	Q_ASSERT(_width  > 2*_verticalBorder);
	Q_ASSERT(_height > 2*_horizontalBorder);
	Q_ASSERT(_width  < 10000);
	Q_ASSERT(_height < 10000);
	
	_mappingType = mappingType;

	// Reserve enough space in the map for the leds
	_colorsMap.reserve(leds.size());

	const unsigned xOffset      = _verticalBorder;
	const unsigned actualWidth  = _width  - 2 * _verticalBorder;
	const unsigned yOffset      = _horizontalBorder;
	const unsigned actualHeight = _height - 2 * _horizontalBorder;
	const unsigned increment    = (_sparseProcessing) ? 2 : 1;
	size_t   totalCount = 0;

	for (const Led& led : leds)
	{
		// skip leds without area
		if ((led.maxX_frac-led.minX_frac) < 1e-6 || (led.maxY_frac-led.minY_frac) < 1e-6)
		{
			_colorsMap.emplace_back();
			continue;
		}

		// Compute the index boundaries for this led
		unsigned minX_idx = xOffset + unsigned(qRound(actualWidth  * led.minX_frac));
		unsigned maxX_idx = xOffset + unsigned(qRound(actualWidth  * led.maxX_frac));
		unsigned minY_idx = yOffset + unsigned(qRound(actualHeight * led.minY_frac));
		unsigned maxY_idx = yOffset + unsigned(qRound(actualHeight * led.maxY_frac));

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
		const auto maxYLedCount = qMin(maxY_idx, yOffset+actualHeight);
		const auto maxXLedCount = qMin(maxX_idx, xOffset+actualWidth);

		std::vector<int32_t> ledColors;
		ledColors.reserve((size_t) maxXLedCount*maxYLedCount);
		if (ImageProcessor::mappingTypeToInt(QString("weighted")) == _mappingType)
		{
			bool left   = led.minX_frac == 0;
			bool right  = led.maxX_frac == 1;			
			bool top    = led.minY_frac == 0;
			bool bottom = led.maxY_frac == 1;	

			int isCorner = ((left) ? 1 : 0) + ((right) ? 1 : 0) + ((top) ? 1 : 0) + ((bottom) ? 1 : 0);

			if (isCorner != 1)
			{
				for (unsigned y = minY_idx; y < maxYLedCount; y += increment)
				{
					for (unsigned x = minX_idx; x < maxXLedCount; x += increment)
					{
						ledColors.push_back(y * width + x);
					}
				}
			}
			else if (bottom)
			{
				unsigned mid = (minY_idx+maxYLedCount)/2;
				for (unsigned y = minY_idx; y < mid; y += increment)
					for (unsigned x = minX_idx; x < maxXLedCount; x += increment)
						ledColors.push_back( -( (int32_t) (y*width + x)));
					
				for (unsigned y = mid; y < maxYLedCount; y += increment)
					for (unsigned x = minX_idx; x < maxXLedCount; x += increment)
						ledColors.push_back(y*width + x);									
			}
			else if (top)
			{
				unsigned mid = (minY_idx+maxYLedCount)/2;
				for (unsigned y = minY_idx; y < mid; y += increment)
					for (unsigned x = minX_idx; x < maxXLedCount; x += increment)
						ledColors.push_back(y*width + x);
					
				for (unsigned y = mid; y < maxYLedCount; y += increment)
					for (unsigned x = minX_idx; x < maxXLedCount; x += increment)
						ledColors.push_back( -( (int32_t) (y*width + x)));
			}

			else if (left)
			{
				unsigned mid = (minX_idx + maxXLedCount)/2;
				for (unsigned y = minY_idx; y < maxYLedCount; y += increment)
				{				
					for (unsigned x = minX_idx; x < mid; x += increment)
						ledColors.push_back(y*width + x);
					for (unsigned x = mid; x < maxXLedCount; x += increment)
						ledColors.push_back( -( (int32_t) (y*width + x)));
				}					
			}

			else if (right)
			{
				unsigned mid = (minX_idx + maxXLedCount)/2;
				for (unsigned y = minY_idx; y < maxYLedCount; y += increment)
				{				
					for (unsigned x = minX_idx; x < mid; x += increment)
						ledColors.push_back( -( (int32_t) (y*width + x)));
					for (unsigned x = mid; x < maxXLedCount; x += increment)
						ledColors.push_back(y*width + x);
				}
			}
								
		}
		else
		{
			for (unsigned y = minY_idx; y < maxYLedCount; y += increment)
			{
				for (unsigned x = minX_idx; x < maxXLedCount; x += increment)
				{
					ledColors.push_back(y*width + x);
				}
			}
		}

		// Add the constructed vector to the map
		_colorsMap.push_back(ledColors);
		totalCount += ledColors.size();
	}
	Info(_log, "Total index number for instance: %d is: %d. Sparse processing: %s, image size: %d x %d, area number: %d",
		_instanceIndex, totalCount, (_sparseProcessing)?"enabled":"disabled", width, height, leds.size()); 
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
				
std::vector<ColorRgb> ImageToLedsMap::Process(const Image<ColorRgb>& image,uint16_t *advanced){
	std::vector<ColorRgb> colors;
	switch (_mappingType)
	{
		case 3:
		case 2: colors = getMeanAdvLedColor(image, advanced); break;
		case 1: colors = getUniLedColor(image); break;
		default: colors = getMeanLedColor(image);
	}
	return colors;
}
		
void ImageToLedsMap::Process(const Image<ColorRgb>& image, std::vector<ColorRgb>& ledColors, uint16_t *advanced){
	switch (_mappingType)
	{
		case 3:
		case 2: getMeanAdvLedColor(image, ledColors, advanced); break;
		case 1: getUniLedColor(image, ledColors); break;
		default: getMeanLedColor(image, ledColors);
	}
}

std::vector<ColorRgb> ImageToLedsMap::getMeanLedColor(const Image<ColorRgb> & image) const
{
	std::vector<ColorRgb> colors(_colorsMap.size(), ColorRgb{0,0,0});
	getMeanLedColor(image, colors);
	return colors;
}

void ImageToLedsMap::getMeanLedColor(const Image<ColorRgb> & image, std::vector<ColorRgb> & ledColors) const
{
	// Sanity check for the number of leds
	//assert(_colorsMap.size() == ledColors.size());
	if(_colorsMap.size() != ledColors.size())
	{
		Debug(Logger::getInstance("HYPERHDR"), "ImageToLedsMap: colorsMap.size != ledColors.size -> %d != %d", _colorsMap.size(), ledColors.size());
		return;
	}

	// Iterate each led and compute the mean
	auto led = ledColors.begin();
	for (auto colors = _colorsMap.begin(); colors != _colorsMap.end(); ++colors, ++led)
	{
		const ColorRgb color = calcMeanColor(image, *colors);
		*led = color;
	}
}

std::vector<ColorRgb> ImageToLedsMap::getUniLedColor(const Image<ColorRgb> & image) const
{
	std::vector<ColorRgb> colors(_colorsMap.size(), ColorRgb{0,0,0});
	getUniLedColor(image, colors);
	return colors;
}

void ImageToLedsMap::getUniLedColor(const Image<ColorRgb> & image, std::vector<ColorRgb> & ledColors) const
{
	// Sanity check for the number of leds
	// assert(_colorsMap.size() == ledColors.size());
	if(_colorsMap.size() != ledColors.size())
	{
		Debug(Logger::getInstance("HYPERHDR"), "ImageToLedsMap: colorsMap.size != ledColors.size -> %d != %d", _colorsMap.size(), ledColors.size());
		return;
	}

	// calculate uni color
	const ColorRgb color = calcMeanColor(image);
	std::fill(ledColors.begin(),ledColors.end(), color);
}
		
std::vector<ColorRgb> ImageToLedsMap::getMeanAdvLedColor(const Image<ColorRgb> & image, uint16_t* lut) const
{
	std::vector<ColorRgb> colors(_colorsMap.size(), ColorRgb{0,0,0});
	getMeanAdvLedColor(image, colors, lut);
	return colors;
}
		
void ImageToLedsMap::getMeanAdvLedColor(const Image<ColorRgb> & image, std::vector<ColorRgb> & ledColors, uint16_t* lut) const
{
	// Sanity check for the number of leds
	//assert(_colorsMap.size() == ledColors.size());
	if(_colorsMap.size() != ledColors.size())
	{
		Debug(Logger::getInstance("HYPERHDR"), "ImageToLedsMap: colorsMap.size != ledColors.size -> %d != %d", _colorsMap.size(), ledColors.size());
		return;
	}

	// Iterate each led and compute the mean
	auto led = ledColors.begin();
	for (auto colors = _colorsMap.begin(); colors != _colorsMap.end(); ++colors, ++led)
	{
		const ColorRgb color = calcMeanAdvColor(image, *colors, lut);
		*led = color;
	}
}

ColorRgb ImageToLedsMap::calcMeanColor(const Image<ColorRgb> & image, const std::vector<int32_t> & colors) const
{
	const auto colorVecSize = colors.size();

	if (colorVecSize == 0)
	{
		return ColorRgb::BLACK;
	}

	// Accumulate the sum of each separate color channel
	uint_fast32_t cummRed   = 0;
	uint_fast32_t cummGreen = 0;
	uint_fast32_t cummBlue  = 0;
	const auto& imgData = image.memptr();

	for (const unsigned colorOffset : colors)
	{
		const auto& pixel = imgData[colorOffset];
		cummRed   += pixel.red;
		cummGreen += pixel.green;
		cummBlue  += pixel.blue;
	}

	// Compute the average of each color channel
	const uint8_t avgRed   = uint8_t(cummRed/colorVecSize);
	const uint8_t avgGreen = uint8_t(cummGreen/colorVecSize);
	const uint8_t avgBlue  = uint8_t(cummBlue/colorVecSize);

	// Return the computed color
	return {avgRed, avgGreen, avgBlue};
}

ColorRgb ImageToLedsMap::calcMeanAdvColor(const Image<ColorRgb> & image, const std::vector<int32_t> & colors, uint16_t* lut) const
{
	const auto colorVecSize = colors.size();

	if (colorVecSize == 0)
	{
		return ColorRgb::BLACK;
	}

	// Accumulate the sum of each seperate color channel
	uint_fast64_t sum1      = 0;
	uint_fast64_t cummRed1   = 0;
	uint_fast64_t cummGreen1 = 0;
	uint_fast64_t cummBlue1  = 0;
			
	uint_fast64_t sum2      = 0;
	uint_fast64_t cummRed2   = 0;
	uint_fast64_t cummGreen2 = 0;
	uint_fast64_t cummBlue2  = 0;
			
	const auto& imgData = image.memptr();

	for (const int32_t colorOffset : colors)
	{
		if (colorOffset >=0) {
			const auto& pixel = imgData[colorOffset];
			cummRed1   += lut[pixel.red];
			cummGreen1 += lut[pixel.green];
			cummBlue1  += lut[pixel.blue];
			sum1++;
		}
		else {
			const auto& pixel = imgData[-colorOffset];
			cummRed2   += lut[pixel.red];
			cummGreen2 += lut[pixel.green];
			cummBlue2  += lut[pixel.blue];
			sum2++;
		}				
	}
			
												
	if (sum1>0 && sum2>0)
	{
		uint16_t avgRed =   std::min((uint32_t)sqrt(((cummRed1  *3)/sum1 + cummRed2  /sum2)/4), (uint32_t)255);
		uint16_t avgGreen = std::min((uint32_t)sqrt(((cummGreen1*3)/sum1 + cummGreen2/sum2)/4), (uint32_t)255);
		uint16_t avgBlue =  std::min((uint32_t)sqrt(((cummBlue1 *3)/sum1 + cummBlue2 /sum2)/4), (uint32_t)255);
			
		return {(uint8_t)avgRed, (uint8_t)avgGreen, (uint8_t)avgBlue};
	}
	else
	{
		uint16_t avgRed =   std::min((uint32_t)sqrt((cummRed1   + cummRed2)  /(sum1 +sum2)), (uint32_t)255);
		uint16_t avgGreen = std::min((uint32_t)sqrt((cummGreen1 + cummGreen2)/(sum1 +sum2)), (uint32_t)255);
		uint16_t avgBlue =  std::min((uint32_t)sqrt((cummBlue1  + cummBlue2) /(sum1 +sum2)), (uint32_t)255);
			
		return {(uint8_t)avgRed, (uint8_t)avgGreen, (uint8_t)avgBlue};		
	}
}

ColorRgb ImageToLedsMap::calcMeanColor(const Image<ColorRgb> & image) const
{
	// Accumulate the sum of each separate color channel
	uint_fast32_t cummRed   = 0;
	uint_fast32_t cummGreen = 0;
	uint_fast32_t cummBlue  = 0;
	const unsigned imageSize = image.width() * image.height();

	const auto& imgData = image.memptr();

	for (unsigned idx=0; idx<imageSize; idx++)
	{
		const auto& pixel = imgData[idx];
		cummRed   += pixel.red;
		cummGreen += pixel.green;
		cummBlue  += pixel.blue;
	}

	// Compute the average of each color channel
	const uint8_t avgRed   = uint8_t(cummRed/imageSize);
	const uint8_t avgGreen = uint8_t(cummGreen/imageSize);
	const uint8_t avgBlue  = uint8_t(cummBlue/imageSize);

	// Return the computed color
	return {avgRed, avgGreen, avgBlue};
}
