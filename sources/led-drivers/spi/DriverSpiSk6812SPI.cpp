/* DriverSpiSk6812SPI.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2026 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/HyperHDR
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
 */

#include <led-drivers/spi/DriverSpiSk6812SPI.h>
#include <infinite-color-engine/ColorSpace.h>

namespace {
	constexpr int SPI_FRAME_END_LATCH_BYTES = 4;
}

DriverSpiSk6812SPI::DriverSpiSk6812SPI(const QJsonObject& deviceConfig)
	: ProviderSpi(deviceConfig)
	, _whiteAlgorithm(RGBW::WhiteAlgorithm::HYPERSERIAL_COLD_WHITE)
	, _white_channel_limit(255)
	, _white_channel_red(255)
	, _white_channel_green(255)
	, _white_channel_blue(255)
	, SPI_BYTES_PER_COLOUR(4)
	, bitpair_to_byte{
		0b10001000,
		0b10001110,
		0b11101000,
		0b11101110}
	, _enable_ice_rgbw(false)
	, _ice_white_temperatur{ 1.0f, 1.0f, 1.0f }
	, _ice_white_mixer_threshold(0.0f)
	, _ice_white_led_intensity(1.8f)
{
}

bool DriverSpiSk6812SPI::init(QJsonObject deviceConfig)
{
	deviceConfig["rate"] = std::max(deviceConfig["rate"].toInt(3200000), 3000000);

	bool isInitOK = false;

	// Initialise sub-class
	if (ProviderSpi::init(deviceConfig))
	{
		QString whiteAlgorithm = deviceConfig["white_algorithm"].toString("hyperserial_cold_white");

		_whiteAlgorithm = RGBW::stringToWhiteAlgorithm(whiteAlgorithm);
		_white_channel_limit = qMin(qRound(deviceConfig["white_channel_limit"].toDouble(1) * 255.0 / 100.0), 255);
		_white_channel_red = qMin(deviceConfig["white_channel_red"].toInt(255), 255);
		_white_channel_green = qMin(deviceConfig["white_channel_green"].toInt(255), 255);
		_white_channel_blue = qMin(deviceConfig["white_channel_blue"].toInt(255), 255);

		_enable_ice_rgbw = deviceConfig["enable_ice_rgbw"].toBool(false);
		_ice_white_mixer_threshold = deviceConfig["ice_white_mixer_threshold"].toDouble(0.0);
		_ice_white_led_intensity = deviceConfig["ice_white_led_intensity"].toDouble(1.8);
		_ice_white_temperatur.x = deviceConfig["ice_white_temperatur_red"].toDouble(1.0);
		_ice_white_temperatur.y = deviceConfig["ice_white_temperatur_green"].toDouble(1.0);
		_ice_white_temperatur.z = deviceConfig["ice_white_temperatur_blue"].toDouble(1.0);
		Debug(_log, "Infinite Color Engine RGBW is: {:s}, white channel temp for the white LED: {:s}, white mixer threshold: {:f}, white LED intensity: {:f}",
			((_enable_ice_rgbw) ? "enabled" : "disabled"), ColorSpaceMath::vecToString(_ice_white_temperatur), _ice_white_mixer_threshold, _ice_white_led_intensity);

		if (_whiteAlgorithm == RGBW::WhiteAlgorithm::INVALID)
		{
			QString errortext = QString("unknown whiteAlgorithm: %1").arg(whiteAlgorithm);
			this->setInError(errortext);
			isInitOK = false;
		}
		else
		{
			Debug(_log, "white_algorithm : {:s}", (whiteAlgorithm));

			if (_whiteAlgorithm == RGBW::WhiteAlgorithm::HYPERSERIAL_CUSTOM)
			{
				Debug(_log, "White channel limit: {:d}, red: {:d}, green: {:d}, blue: {:d}", _white_channel_limit, _white_channel_red, _white_channel_green, _white_channel_blue);
			}

			if (_whiteAlgorithm == RGBW::WhiteAlgorithm::HYPERSERIAL_CUSTOM ||
				_whiteAlgorithm == RGBW::WhiteAlgorithm::HYPERSERIAL_NEUTRAL_WHITE ||
				_whiteAlgorithm == RGBW::WhiteAlgorithm::HYPERSERIAL_COLD_WHITE)
			{
				RGBW::prepareRgbwCalibration(channelCorrection, _whiteAlgorithm, _white_channel_limit, _white_channel_red, _white_channel_green, _white_channel_blue);
			}

			auto rateHz = getRate();

			WarningIf((rateHz < 3200000 || rateHz > 3334000), _log, "Real SPI rate {:d} outside recommended range (3200000-3334000, ideal: 3200000)", rateHz);

			_ledBuffer.assign(_ledRGBWCount * SPI_BYTES_PER_COLOUR + SPI_FRAME_END_LATCH_BYTES, 0x00);

			isInitOK = true;
		}
	}
	return isInitOK;
}

int DriverSpiSk6812SPI::writeFiniteColors(const std::vector<ColorRgb>& ledValues)
{
	unsigned spi_ptr = 0;
	const int SPI_BYTES_PER_LED = sizeof(ColorRgbw) * SPI_BYTES_PER_COLOUR;

	if (_ledCount != ledValues.size())
	{
		Warning(_log, "Sk6812SPI led's number has changed (old: {:d}, new: {:d}). Rebuilding buffer.", _ledCount, ledValues.size());
		_ledCount = static_cast<uint>(ledValues.size());

		_ledBuffer.assign(SPI_BYTES_PER_LED * _ledCount + SPI_FRAME_END_LATCH_BYTES, 0x00);
	}

	for (const ColorRgb& color : ledValues)
	{
		ColorRgbw tempRgbw;

		RGBW::rgb2rgbw(color, &tempRgbw, _whiteAlgorithm, channelCorrection);
		uint32_t colorBits =
			((uint32_t)tempRgbw.red << 24) +
			((uint32_t)tempRgbw.green << 16) +
			((uint32_t)tempRgbw.blue << 8) +
			tempRgbw.white;

		for (int j = SPI_BYTES_PER_LED - 1; j >= 0; j--)
		{
			_ledBuffer[spi_ptr + j] = bitpair_to_byte[colorBits & 0x3];
			colorBits >>= 2;
		}
		spi_ptr += SPI_BYTES_PER_LED;
	}

	return writeBytes(static_cast<unsigned int>(_ledBuffer.size()), _ledBuffer.data());
}

std::pair<bool, int> DriverSpiSk6812SPI::writeInfiniteColors(SharedOutputColors nonlinearRgbColors)
{
	if (nonlinearRgbColors->empty() || !_enable_ice_rgbw)
	{
		return { _enable_ice_rgbw, 0 };
	}

	unsigned spi_ptr = 0;
	const int SPI_BYTES_PER_LED = sizeof(ColorRgbw) * SPI_BYTES_PER_COLOUR;

	if (_ledCount != nonlinearRgbColors->size())
	{
		Warning(_log, "Sk6812SPI led's number has changed (old: {:d}, new: {:d}). Rebuilding buffer.", _ledCount, nonlinearRgbColors->size());
		_ledCount = static_cast<uint>(nonlinearRgbColors->size());

		_ledBuffer.assign(SPI_BYTES_PER_LED * _ledCount + SPI_FRAME_END_LATCH_BYTES, 0x00);
	}

	////////////////////////////////////////////////////////////////////////////

	_infColors.resize(nonlinearRgbColors->size() * 4);

	// RGBW by Infinite Color Engine
	_infiniteColorEngineRgbw.renderRgbwFrame(*nonlinearRgbColors, _currentInterval, _ice_white_mixer_threshold, _ice_white_led_intensity, _ice_white_temperatur, _infColors, 0, _colorOrder);

	auto start = _infColors.data();
	auto end = start + _infColors.size() - 4;

	for (uint8_t* current = start; current <= end; current += 4)
	{
		uint32_t colorBits =
			((uint32_t)current[0] << 24) +
			((uint32_t)current[1] << 16) +
			((uint32_t)current[2] << 8) +
			current[3];

		for (int j = SPI_BYTES_PER_LED - 1; j >= 0; j--)
		{
			_ledBuffer[spi_ptr + j] = bitpair_to_byte[colorBits & 0x3];
			colorBits >>= 2;
		}
		spi_ptr += SPI_BYTES_PER_LED;
	}

	return { true, writeBytes(static_cast<unsigned int>(_ledBuffer.size()), _ledBuffer.data()) };
}

LedDevice* DriverSpiSk6812SPI::construct(const QJsonObject& deviceConfig)
{
	return new DriverSpiSk6812SPI(deviceConfig);
}

bool DriverSpiSk6812SPI::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("sk6812spi", "leds_group_0_SPI", DriverSpiSk6812SPI::construct);
