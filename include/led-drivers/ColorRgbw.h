#pragma once

#ifndef PCH_ENABLED
	#include <QString>

	#include <cstdint>
	#include <iostream>	
#endif

#include <image/ColorRgb.h>

struct ColorRgbw
{
	uint8_t red;
	uint8_t green;
	uint8_t blue;
	uint8_t white;

	static const ColorRgbw BLACK;
	static const ColorRgbw RED;
	static const ColorRgbw GREEN;
	static const ColorRgbw BLUE;
	static const ColorRgbw YELLOW;
	static const ColorRgbw WHITE;
};

static_assert(sizeof(ColorRgbw) == 4, "Incorrect size of ColorRgbw");

inline std::ostream& operator<<(std::ostream& os, const ColorRgbw& color)
{
	os << "{"
		<< color.red << ","
		<< color.green << ","
		<< color.blue << ","
		<< color.white <<
		"}";

	return os;
}

inline bool operator==(const ColorRgbw& lhs, const ColorRgbw& rhs)
{
	return	lhs.red == rhs.red &&
		lhs.green == rhs.green &&
		lhs.blue == rhs.blue &&
		lhs.white == rhs.white;
}

inline bool operator<(const ColorRgbw& lhs, const ColorRgbw& rhs)
{
	return	lhs.red < rhs.red&&
		lhs.green < rhs.green&&
		lhs.blue < rhs.blue&&
		lhs.white < rhs.white;
}

inline bool operator<=(const ColorRgbw& lhs, const ColorRgbw& rhs)
{
	return lhs < rhs || lhs == rhs;
}

namespace RGBW {

	enum class WhiteAlgorithm {
		INVALID,
		SUBTRACT_MINIMUM,
		SUB_MIN_WARM_ADJUST,
		SUB_MIN_COOL_ADJUST,
		WHITE_OFF,
		HYPERSERIAL_COLD_WHITE,
		HYPERSERIAL_NEUTRAL_WHITE,
		HYPERSERIAL_CUSTOM,
		WLED_AUTO,
		WLED_AUTO_MAX,
		WLED_AUTO_ACCURATE
	};

	struct RgbwChannelCorrection
	{
		uint8_t white[256];
		uint8_t red[256];
		uint8_t green[256];
		uint8_t blue[256];
	};


	WhiteAlgorithm stringToWhiteAlgorithm(const QString& str);
	void prepareRgbwCalibration(RgbwChannelCorrection& channelCorrection, WhiteAlgorithm algorithm, uint8_t gain, uint8_t red, uint8_t green, uint8_t blue);
	void rgb2rgbw(ColorRgb input, ColorRgbw* output, WhiteAlgorithm algorithm, const RgbwChannelCorrection& channelCorrection);
}
