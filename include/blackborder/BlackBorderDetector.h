//#include <iostream>
#pragma once

// Utils includes
#include <utils/Image.h>

namespace hyperhdr
{
	///
	/// Result structure of the detected blackborder.
	///
	struct BlackBorder
	{
		/// Flag indicating if the border is unknown
		bool unknown;

		/// The size of the detected horizontal border
		int horizontalSize;

		/// The size of the detected vertical border
		int verticalSize;

		///
		/// Compares this BlackBorder to the given other BlackBorder
		///
		/// @param[in] other  The other BlackBorder
		///
		/// @return True if this is the same border as other
		///
		bool operator== (const BlackBorder& other) const;
	};

	///
	/// The BlackBorderDetector performs detection of black-borders on a single image.
	/// The detector will search for the upper left corner of the picture in the frame.
	/// Based on detected black pixels it will give an estimate of the black-border.
	///
	class BlackBorderDetector
	{
	public:
		///
		/// Constructs a black-border detector
		/// @param[in] threshold The threshold which the black-border detector should use
		///
		BlackBorderDetector(double threshold);

		///
		/// Performs the actual black-border detection on the given image
		///
		/// @param[in] image  The image on which detection is performed
		///
		/// @return The detected (or not detected) black border info
		///

		uint8_t calculateThreshold(double blackborderThreshold) const;

		///
		/// default detection mode (3lines 4side detection)
		BlackBorder process(const Image<ColorRgb>& image) const;

		///
		/// classic detection mode (topleft single line mode)
		BlackBorder process_classic(const Image<ColorRgb>& image) const;


		///
		/// osd detection mode (find x then y at detected x to avoid changes by osd overlays)
		BlackBorder process_osd(const Image<ColorRgb>& image) const;


		///
		/// letterbox detection mode (5lines top-bottom only detection)
		BlackBorder process_letterbox(const Image<ColorRgb>& image) const;



	private:

		///
		/// Checks if a given color is considered black and therefore could be part of the border.
		///
		/// @param[in] color  The color to check
		///
		/// @return True if the color is considered black else false
		///

		inline bool isBlack(const ColorRgb& color) const
		{
			// Return the simple compare of the color against black
			return (color.red < _blackborderThreshold) && (color.green < _blackborderThreshold) && (color.blue < _blackborderThreshold);
		}

	private:
		/// Threshold for the black-border detector [0 .. 255]
		const uint8_t _blackborderThreshold;

	};
} // end namespace hyperhdr
