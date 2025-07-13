/* SpiDeviceHD108.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2025 awawa-dev
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

#include <led-drivers/spi/DriverSpiHD108.h>

// Local HyperHDR includes
#include <utils/Logger.h>
#include <cmath>
#include <algorithm>
#include <bit>
#include <linalg.h>

namespace {
	constexpr size_t HD108_START_FRAME_SIZE = 16;  // 128 bits
	constexpr size_t HD108_LED_FRAME_SIZE = 8;     // 64 bits
	const int HD108_MAX_LEVEL = 0x1F;
	const uint16_t HD108_MAX_LEVEL_BIT = 0b1000000000000000;
}

DriverSpiHD108::DriverSpiHD108(const QJsonObject& deviceConfig)
	: ProviderSpi(deviceConfig)
	, _globalBrightnessControlMaxLevel(HD108_MAX_LEVEL)
{
}

bool DriverSpiHD108::init(QJsonObject deviceConfig)
{
	bool isInitOK = false;

	// Initialise sub-class
	if (ProviderSpi::init(deviceConfig))
	{
		_globalBrightnessControlMaxLevel = std::min(deviceConfig["globalBrightnessControlMaxLevel"].toInt(HD108_MAX_LEVEL), HD108_MAX_LEVEL);
		Info(_log, "[HD108] Using global brightness control with max level of %d", _globalBrightnessControlMaxLevel);

		_ledBuffer.clear();
		_ledBuffer.resize((_ledCount * HD108_LED_FRAME_SIZE) + HD108_START_FRAME_SIZE, 0x00);

		isInitOK = true;
	}
	return isInitOK;
}

static inline uint16_t MSB_FIRST(uint16_t v)
{
	if constexpr (std::endian::native == std::endian::little)
		return static_cast<uint16_t>((v >> 8) | (v << 8));
	else
		return v;
}

std::pair<bool, int> DriverSpiHD108::writeInfiniteColors(SharedOutputColors nonlinearRgbColors)
{
	if (_ledCount != nonlinearRgbColors->size())
	{
		Warning(_log, "HD108 led's number has changed (old: %d, new: %d). Rebuilding buffer.", _ledCount, nonlinearRgbColors->size());
		_ledCount = static_cast<uint>(nonlinearRgbColors->size());
		_ledBuffer.clear();
		_ledBuffer.resize((_ledCount * HD108_LED_FRAME_SIZE) + HD108_START_FRAME_SIZE, 0x00);
	}

	int index = HD108_START_FRAME_SIZE;
	uint16_t* data = reinterpret_cast<uint16_t*>(_ledBuffer.data());
	for (auto const& rgb : *nonlinearRgbColors)
	{
		const bool isLit = (rgb.x > 0.0f || rgb.y > 0.0f || rgb.z > 0.0f);
		uint16_t red = static_cast<uint16_t>(std::round(rgb.x * 0xffff));
		uint16_t green = static_cast<uint16_t>(std::round(rgb.y * 0xffff));
		uint16_t blue = static_cast<uint16_t>(std::round(rgb.z * 0xffff));
		uint16_t level = HD108_MAX_LEVEL_BIT;

		for (int i = 0; i < 3 && isLit; i++)
		{
			level |= (_globalBrightnessControlMaxLevel & HD108_MAX_LEVEL) << (5 * i);
		}

		uint16_t level_be = MSB_FIRST(level);
		uint16_t red_be = MSB_FIRST(red);
		uint16_t green_be = MSB_FIRST(green);
		uint16_t blue_be = MSB_FIRST(blue);

		memcpy(_ledBuffer.data() + index, &level_be, sizeof(level_be));
		memcpy(_ledBuffer.data() + index + 2, &red_be, sizeof(red_be));
		memcpy(_ledBuffer.data() + index + 4, &green_be, sizeof(green_be));
		memcpy(_ledBuffer.data() + index + 6, &blue_be, sizeof(blue_be));

		index += HD108_LED_FRAME_SIZE;
	}

	return std::pair<bool,int>(true, writeBytes(_ledBuffer.size(), _ledBuffer.data()));
}

LedDevice* DriverSpiHD108::construct(const QJsonObject& deviceConfig)
{
	return new DriverSpiHD108(deviceConfig);
}

bool DriverSpiHD108::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("hd108", "leds_group_0_SPI", DriverSpiHD108::construct);
