
#pragma once

// STL includes
#include <cassert>
#include <sstream>
#include <math.h>
#include <algorithm>

// hyperion-utils includes
#include <utils/Image.h>
#include <utils/Logger.h>

// hyperion includes
#include <hyperion/LedString.h>

namespace hyperion
{

	///
	/// The ImageToLedsMap holds a mapping of indices into an image to leds. It can be used to
	/// calculate the average (or mean) color per led for a specific region.
	///
	class ImageToLedsMap
	{
	public:

		///
		/// Constructs an mapping from the absolute indices in an image to each led based on the border
		/// definition given in the list of leds. The map holds absolute indices to any given image,
		/// provided that it is row-oriented.
		/// The mapping is created purely on size (width and height). The given borders are excluded
		/// from indexing.
		///
		/// @param[in] width            The width of the indexed image
		/// @param[in] height           The width of the indexed image
		/// @param[in] horizontalBorder The size of the horizontal border (0=no border)
		/// @param[in] verticalBorder   The size of the vertical border (0=no border)
		/// @param[in] leds             The list with led specifications
		///
		ImageToLedsMap(
				const int mappingType,
				const unsigned width,
				const unsigned height,
				const unsigned horizontalBorder,
				const unsigned verticalBorder,
				const std::vector<Led> & leds);

		///
		/// Returns the width of the indexed image
		///
		/// @return The width of the indexed image [pixels]
		///
		unsigned width() const;

		///
		/// Returns the height of the indexed image
		///
		/// @return The height of the indexed image [pixels]
		///
		unsigned height() const;

		unsigned horizontalBorder() const { return _horizontalBorder; }
		unsigned verticalBorder() const { return _verticalBorder; }
		
		template <typename Pixel_T>
		std::vector<ColorRgb> Process(const Image<Pixel_T>& image,uint16_t *advanced){
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
		
		template <typename Pixel_T>
		void Process(const Image<Pixel_T>& image, std::vector<ColorRgb>& ledColors, uint16_t *advanced){
			switch (_mappingType)
			{
				case 3:
				case 2: getMeanAdvLedColor(image, ledColors, advanced); break;
				case 1: getUniLedColor(image, ledColors); break;
				default: getMeanLedColor(image, ledColors);
			}
		}
	private:
		///
		/// Determines the mean color for each led using the mapping the image given
		/// at construction.
		///
		/// @param[in] image  The image from which to extract the led colors
		///
		/// @return ledColors  The vector containing the output
		///
		template <typename Pixel_T>
		std::vector<ColorRgb> getMeanLedColor(const Image<Pixel_T> & image) const
		{
			std::vector<ColorRgb> colors(_colorsMap.size(), ColorRgb{0,0,0});
			getMeanLedColor(image, colors);
			return colors;
		}
	
		///
		/// Determines the mean color for each led using the mapping the image given
		/// at construction.
		///
		/// @param[in] image  The image from which to extract the led colors
		/// @param[out] ledColors  The vector containing the output
		///
		template <typename Pixel_T>
		void getMeanLedColor(const Image<Pixel_T> & image, std::vector<ColorRgb> & ledColors) const
		{
			// Sanity check for the number of leds
			//assert(_colorsMap.size() == ledColors.size());
			if(_colorsMap.size() != ledColors.size())
			{
				Debug(Logger::getInstance("HYPERION"), "ImageToLedsMap: colorsMap.size != ledColors.size -> %d != %d", _colorsMap.size(), ledColors.size());
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

		///
		/// Determines the uni color for each led using the mapping the image given
		/// at construction.
		///
		/// @param[in] image  The image from which to extract the led colors
		///
		/// @return ledColors  The vector containing the output
		///
		template <typename Pixel_T>
		std::vector<ColorRgb> getUniLedColor(const Image<Pixel_T> & image) const
		{
			std::vector<ColorRgb> colors(_colorsMap.size(), ColorRgb{0,0,0});
			getUniLedColor(image, colors);
			return colors;
		}

		///
		/// Determines the uni color for each led using the mapping the image given
		/// at construction.
		///
		/// @param[in] image  The image from which to extract the led colors
		/// @param[out] ledColors  The vector containing the output
		///
		template <typename Pixel_T>
		void getUniLedColor(const Image<Pixel_T> & image, std::vector<ColorRgb> & ledColors) const
		{
			// Sanity check for the number of leds
			// assert(_colorsMap.size() == ledColors.size());
			if(_colorsMap.size() != ledColors.size())
			{
				Debug(Logger::getInstance("HYPERION"), "ImageToLedsMap: colorsMap.size != ledColors.size -> %d != %d", _colorsMap.size(), ledColors.size());
				return;
			}


			// calculate uni color
			const ColorRgb color = calcMeanColor(image);
			std::fill(ledColors.begin(),ledColors.end(), color);
		}
		
		template <typename Pixel_T>
		std::vector<ColorRgb> getMeanAdvLedColor(const Image<Pixel_T> & image, uint16_t* lut) const
		{
			std::vector<ColorRgb> colors(_colorsMap.size(), ColorRgb{0,0,0});
			getMeanAdvLedColor(image, colors, lut);
			return colors;
		}
		
		template <typename Pixel_T>
		void getMeanAdvLedColor(const Image<Pixel_T> & image, std::vector<ColorRgb> & ledColors, uint16_t* lut) const
		{
			// Sanity check for the number of leds
			//assert(_colorsMap.size() == ledColors.size());
			if(_colorsMap.size() != ledColors.size())
			{
				Debug(Logger::getInstance("HYPERION"), "ImageToLedsMap: colorsMap.size != ledColors.size -> %d != %d", _colorsMap.size(), ledColors.size());
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


		/// The width of the indexed image
		const unsigned _width;
		/// The height of the indexed image
		const unsigned _height;

		const unsigned _horizontalBorder;

		const unsigned _verticalBorder;
		
		int _mappingType;

		/// The absolute indices into the image for each led
		std::vector<std::vector<int32_t>> _colorsMap;

		///
		/// Calculates the 'mean color' of the given list. This is the mean over each color-channel
		/// (red, green, blue)
		///
		/// @param[in] image The image a section from which an average color must be computed
		/// @param[in] colors  The list with colors
		///
		/// @return The mean of the given list of colors (or black when empty)
		///
		template <typename Pixel_T>
		ColorRgb calcMeanColor(const Image<Pixel_T> & image, const std::vector<int32_t> & colors) const
		{
			const auto colorVecSize = colors.size();

			if (colorVecSize == 0)
			{
				return ColorRgb::BLACK;
			}

			// Accumulate the sum of each seperate color channel
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

		template <typename Pixel_T>
		ColorRgb calcMeanAdvColor(const Image<Pixel_T> & image, const std::vector<int32_t> & colors, uint16_t* lut) const
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
		///
		/// Calculates the 'mean color' over the given image. This is the mean over each color-channel
		/// (red, green, blue)
		///
		/// @param[in] image The image a section from which an average color must be computed
		///
		/// @return The mean of the given list of colors (or black when empty)
		///
		template <typename Pixel_T>
		ColorRgb calcMeanColor(const Image<Pixel_T> & image) const
		{
			// Accumulate the sum of each seperate color channel
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
	};

} // end namespace hyperion
