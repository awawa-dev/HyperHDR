#include <led-drivers/ColorRgbw.h>

const ColorRgbw ColorRgbw::BLACK  = {   0,   0,   0,   0 };
const ColorRgbw ColorRgbw::RED    = { 255,   0,   0,   0 };
const ColorRgbw ColorRgbw::GREEN  = {   0, 255,   0,   0 };
const ColorRgbw ColorRgbw::BLUE   = {   0,   0, 255,   0 };
const ColorRgbw ColorRgbw::YELLOW = { 255, 255,   0,   0 };
const ColorRgbw ColorRgbw::WHITE  = {   0,   0,   0, 255 };

#define ROUND_DIVIDE(number, denom) (((number) + (denom) / 2) / (denom))

namespace RGBW {

	WhiteAlgorithm stringToWhiteAlgorithm(const QString& str)
	{
		if (str == "subtract_minimum")
		{
			return WhiteAlgorithm::SUBTRACT_MINIMUM;
		}
		if (str == "sub_min_warm_adjust")
		{
			return WhiteAlgorithm::SUB_MIN_WARM_ADJUST;
		}
		if (str == "sub_min_cool_adjust")
		{
			return WhiteAlgorithm::SUB_MIN_COOL_ADJUST;
		}
		if (str == "hyperserial_cold_white")
		{
			return WhiteAlgorithm::HYPERSERIAL_COLD_WHITE;
		}
		if (str == "hyperserial_neutral_white")
		{
			return WhiteAlgorithm::HYPERSERIAL_NEUTRAL_WHITE;
		}
		if (str == "hyperserial_custom")
		{
			return WhiteAlgorithm::HYPERSERIAL_CUSTOM;
		}
		if (str == "wled_auto")
		{
			return WhiteAlgorithm::WLED_AUTO;
		}
		if (str == "wled_auto_max")
		{
			return WhiteAlgorithm::WLED_AUTO_MAX;
		}
		if (str == "wled_auto_accurate")
		{
			return WhiteAlgorithm::WLED_AUTO_ACCURATE;
		}

		if (str.isEmpty() || str == "white_off")
		{
			return WhiteAlgorithm::WHITE_OFF;
		}
		return WhiteAlgorithm::INVALID;
	}

	void prepareRgbwCalibration(RgbwChannelCorrection& channelCorrection, WhiteAlgorithm algorithm, uint8_t gain, uint8_t red, uint8_t green, uint8_t blue)
	{
		if (algorithm == WhiteAlgorithm::HYPERSERIAL_COLD_WHITE)
		{
			gain = 0xFF;
			red = 0xA0;
			green = 0xA0;
			blue = 0xA0;
		}
		else if (algorithm == WhiteAlgorithm::HYPERSERIAL_NEUTRAL_WHITE) {
			gain = 0xFF;
			red = 0xB0;
			green = 0xB0;
			blue = 0x70;
		}
		else if (algorithm != WhiteAlgorithm::HYPERSERIAL_CUSTOM)
		{
			return;
		}

		// my HyperSerialESP32 code
		// prepare LUT calibration table, cold white is much better than "neutral" white
		for (uint32_t i = 0; i < 256; i++)
		{
			// color calibration
			uint32_t _gain = gain * i;   // adjust gain
			uint32_t _red = red * i;     // adjust red
			uint32_t _green = green * i; // adjust green
			uint32_t _blue = blue * i;   // adjust blue

			channelCorrection.white[i] = (uint8_t)std::min(ROUND_DIVIDE(_gain, 0xFF), (uint32_t)0xFF);
			channelCorrection.red[i] = (uint8_t)std::min(ROUND_DIVIDE(_red, 0xFF), (uint32_t)0xFF);
			channelCorrection.green[i] = (uint8_t)std::min(ROUND_DIVIDE(_green, 0xFF), (uint32_t)0xFF);
			channelCorrection.blue[i] = (uint8_t)std::min(ROUND_DIVIDE(_blue, 0xFF), (uint32_t)0xFF);
		}
	}

	void rgb2rgbw(ColorRgb input, ColorRgbw* output, WhiteAlgorithm algorithm, const RgbwChannelCorrection& channelCorrection)
	{
		switch (algorithm)
		{
		case WhiteAlgorithm::SUBTRACT_MINIMUM:
		{
			output->white = static_cast<uint8_t>(qMin(qMin(input.red, input.green), input.blue));
			output->red = input.red - output->white;
			output->green = input.green - output->white;
			output->blue = input.blue - output->white;
			break;
		}

		case WhiteAlgorithm::SUB_MIN_WARM_ADJUST:
		{
			// http://forum.garagecube.com/viewtopic.php?t=10178
			// warm white
			const double F1(0.274);
			const double F2(0.454);
			const double F3(2.333);

			output->white = static_cast<uint8_t>(qMin(input.red * F1, qMin(input.green * F2, input.blue * F3)));
			output->red = input.red - static_cast<uint8_t>(output->white / F1);
			output->green = input.green - static_cast<uint8_t>(output->white / F2);
			output->blue = input.blue - static_cast<uint8_t>(output->white / F3);
			break;
		}

		case WhiteAlgorithm::SUB_MIN_COOL_ADJUST:
		{
			// http://forum.garagecube.com/viewtopic.php?t=10178
			// cold white
			const double F1(0.299);
			const double F2(0.587);
			const double F3(0.114);

			output->white = static_cast<uint8_t>(qMin(input.red * F1, qMin(input.green * F2, input.blue * F3)));
			output->red = input.red - static_cast<uint8_t>(output->white / F1);
			output->green = input.green - static_cast<uint8_t>(output->white / F2);
			output->blue = input.blue - static_cast<uint8_t>(output->white / F3);
			break;
		}

		case WhiteAlgorithm::WHITE_OFF:
		{
			output->red = input.red;
			output->green = input.green;
			output->blue = input.blue;
			output->white = 0;
			break;
		}

		case WhiteAlgorithm::WLED_AUTO_MAX:
		{
			output->red = input.red;
			output->green = input.green;
			output->blue = input.blue;
			output->white = input.red > input.green ? (input.red > input.blue ? input.red : input.blue) : (input.green > input.blue ? input.green : input.blue);
			break;
		}

		case WhiteAlgorithm::WLED_AUTO_ACCURATE:
		{
			output->white = input.red < input.green ? (input.red < input.blue ? input.red : input.blue) : (input.green < input.blue ? input.green : input.blue);
			output->red = input.red - output->white;
			output->green = input.green - output->white;
			output->blue = input.blue - output->white;
			break;
		}

		case WhiteAlgorithm::WLED_AUTO:
		{

			output->red = input.red;
			output->green = input.green;
			output->blue = input.blue;
			output->white = input.red < input.green ? (input.red < input.blue ? input.red : input.blue) : (input.green < input.blue ? input.green : input.blue);
			break;
		}

		case WhiteAlgorithm::HYPERSERIAL_NEUTRAL_WHITE:
		case WhiteAlgorithm::HYPERSERIAL_COLD_WHITE:
		case WhiteAlgorithm::HYPERSERIAL_CUSTOM:
		{
			auto white = std::min(channelCorrection.red[input.red],
				std::min(channelCorrection.green[input.green],
					channelCorrection.blue[input.blue]));
			output->red = input.red - channelCorrection.red[white];
			output->green = input.green - channelCorrection.green[white];
			output->blue = input.blue - channelCorrection.blue[white];
			output->white = channelCorrection.white[white];
			break;
		}
		default:
			break;
		}
	}

};
