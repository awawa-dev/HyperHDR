
#pragma once

// STL includes
#include <cassert>
#include <sstream>
#include <math.h>
#include <algorithm>


#include <utils/Image.h>
#include <utils/Logger.h>


#include <base/LedString.h>

namespace hyperhdr
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
			Logger* _log,
			const int mappingType,
			const bool sparseProcessing,
			const unsigned width,
			const unsigned height,
			const unsigned horizontalBorder,
			const unsigned verticalBorder,
			const quint8 instanceIndex,
			const std::vector<Led>& leds);

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

		unsigned horizontalBorder() const;
		unsigned verticalBorder() const;

		std::vector<ColorRgb> Process(const Image<ColorRgb>& image, uint16_t* advanced);

	private:
		std::vector<ColorRgb> getMeanLedColor(const Image<ColorRgb>& image) const;

		std::vector<ColorRgb> getUniLedColor(const Image<ColorRgb>& image) const;

		std::vector<ColorRgb> getMeanAdvLedColor(const Image<ColorRgb>& image, uint16_t* lut) const;

		/// The width of the indexed image
		const unsigned _width;
		/// The height of the indexed image
		const unsigned _height;

		const unsigned _horizontalBorder;

		const unsigned _verticalBorder;

		int _mappingType;

		/// The absolute indices into the image for each led
		std::vector<std::vector<int32_t>> _colorsMap;
		std::vector<bool> _colorsDisabled;
		std::vector<int> _colorsGroups;

		int _groupMin;
		int _groupMax;
		bool _haveDisabled;

		ColorRgb calcMeanColor(const Image<ColorRgb>& image, const std::vector<int32_t>& colors) const;

		ColorRgb calcMeanAdvColor(const Image<ColorRgb>& image, const std::vector<int32_t>& colors, uint16_t* lut) const;

		ColorRgb calcMeanColor(const Image<ColorRgb>& image) const;
	};

}
