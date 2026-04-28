/* DriverSpiWs2812SPI.cpp
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

#include <led-drivers/spi/DriverSpiWs2812SPI.h>

namespace {
	constexpr int SPI_FRAME_END_LATCH_BYTES = 4;
}

DriverSpiWs2812SPI::DriverSpiWs2812SPI(const QJsonObject& deviceConfig)
	: ProviderSpi(deviceConfig)
	, SPI_BYTES_PER_COLOUR(4)
	, bitpair_to_byte{
		0b10001000,
		0b10001110,
		0b11101000,
		0b11101110
	}
{
}

bool DriverSpiWs2812SPI::init(QJsonObject deviceConfig)
{
	deviceConfig["rate"] = std::max(deviceConfig["rate"].toInt(3200000), 3000000);

	bool isInitOK = false;

	// Initialise sub-class
	if (ProviderSpi::init(deviceConfig))
	{
		auto rateHz = getRate();

		WarningIf((rateHz < 3000000 || rateHz > 3334000), _log, "Real SPI rate {:d} outside of recommended range (3000000-3334000, ideal: 3200000)", rateHz);

		_ledBuffer.resize(_ledRGBCount * SPI_BYTES_PER_COLOUR + SPI_FRAME_END_LATCH_BYTES, 0x00);

		isInitOK = true;
	}

	return isInitOK;
}

int DriverSpiWs2812SPI::writeFiniteColors(const std::vector<ColorRgb>& ledValues)
{
	unsigned spi_ptr = 0;
	const int SPI_BYTES_PER_LED = sizeof(ColorRgb) * SPI_BYTES_PER_COLOUR;

	if (_ledCount != ledValues.size())
	{
		Warning(_log, "Ws2812SPI led's number has changed (old: {:d}, new: {:d}). Rebuilding buffer.", _ledCount, ledValues.size());
		_ledCount = static_cast<uint>(ledValues.size());

		_ledBuffer.assign(SPI_BYTES_PER_LED * _ledCount + SPI_FRAME_END_LATCH_BYTES, 0x00);
	}

	for (const ColorRgb& color : ledValues)
	{
		uint32_t colorBits = ((unsigned int)color.red << 16)
			| ((unsigned int)color.green << 8)
			| color.blue;

		for (int j = SPI_BYTES_PER_LED - 1; j >= 0; j--)
		{
			_ledBuffer[spi_ptr + j] = bitpair_to_byte[colorBits & 0x3];
			colorBits >>= 2;
		}
		spi_ptr += SPI_BYTES_PER_LED;
	}

	return writeBytes(static_cast<unsigned int>(_ledBuffer.size()), _ledBuffer.data());
}

LedDevice* DriverSpiWs2812SPI::construct(const QJsonObject& deviceConfig)
{
	return new DriverSpiWs2812SPI(deviceConfig);
}

bool DriverSpiWs2812SPI::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("ws2812spi", "leds_group_0_SPI", DriverSpiWs2812SPI::construct);
