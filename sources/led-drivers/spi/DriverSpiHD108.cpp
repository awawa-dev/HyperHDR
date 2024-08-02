/* SpiDeviceHD108.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2024 awawa-dev
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

namespace {
	const int HD108_MAX_LEVEL = 0x1F;
	const uint32_t HD108_MAX_THRESHOLD = 100u;
	const uint32_t HD108_MAX_THRESHOLD_2 = HD108_MAX_THRESHOLD * HD108_MAX_THRESHOLD;
	const uint16_t HD108_MAX_LEVEL_BIT = 0b1000000000000000;
}

DriverSpiHD108::DriverSpiHD108(const QJsonObject& deviceConfig)
	: ProviderSpi(deviceConfig)
	, _globalBrightnessControlThreshold(HD108_MAX_THRESHOLD)
	, _globalBrightnessControlMaxLevel(HD108_MAX_LEVEL)
{
}

LedDevice* DriverSpiHD108::construct(const QJsonObject& deviceConfig)
{
	return new DriverSpiHD108(deviceConfig);
}

bool DriverSpiHD108::init(const QJsonObject& deviceConfig)
{
	bool isInitOK = false;

	// Initialise sub-class
	if (ProviderSpi::init(deviceConfig))
	{
		_globalBrightnessControlThreshold = static_cast<uint32_t>(std::min(
			std::lround(deviceConfig["globalBrightnessControlThreshold"].toDouble(HD108_MAX_THRESHOLD) * HD108_MAX_THRESHOLD),
			(long)HD108_MAX_THRESHOLD_2));
		_globalBrightnessControlMaxLevel = deviceConfig["globalBrightnessControlMaxLevel"].toInt(HD108_MAX_LEVEL);
		Info(_log, "[HD108] Using global brightness control with threshold of %d and max level of %d", _globalBrightnessControlThreshold, _globalBrightnessControlMaxLevel);

		_ledBuffer.resize(0, 0x00);
		_ledBuffer.resize((_ledCount * 8) + 16, 0x00);

		isInitOK = true;
	}
	return isInitOK;
}

static inline uint16_t MSB_FIRST(uint16_t x, bool littleEndian)
{	
	if (littleEndian)
		return (x >> 8) | (x << 8);
	else
		return x;
}

int DriverSpiHD108::write(const std::vector<ColorRgb>& ledValues)
{
	int littleEndian = 1;

	if (*(char*)&littleEndian != 1)
		littleEndian = 0;


	if (_ledCount != ledValues.size())
	{
		Warning(_log, "HD108 led's number has changed (old: %d, new: %d). Rebuilding buffer.", _ledCount, ledValues.size());
		_ledCount = static_cast<uint>(ledValues.size());
		_ledBuffer.resize(0, 0x00);
		_ledBuffer.resize((_ledCount * 8) + 16, 0x00);
	}


	int index = 8;
	uint16_t* data = reinterpret_cast<uint16_t*>(_ledBuffer.data());
	for (auto const& rgb : ledValues)
	{
		const int isLit = (rgb.red || rgb.green || rgb.blue);
		uint16_t red = rgb.red * 256u;
		uint16_t green = rgb.green * 256u;
		uint16_t blue = rgb.blue * 256u;
		uint16_t level = HD108_MAX_LEVEL_BIT;

		for (int i = 0; i < 3 && isLit; i++)
		{
			level |= (_globalBrightnessControlMaxLevel & HD108_MAX_LEVEL) << (5 * i);
		}

		if (_globalBrightnessControlThreshold < HD108_MAX_THRESHOLD_2)
		{
			red = (red * _globalBrightnessControlThreshold) / HD108_MAX_THRESHOLD_2;
			green = (green * _globalBrightnessControlThreshold) / HD108_MAX_THRESHOLD_2;
			blue = (blue * _globalBrightnessControlThreshold) / HD108_MAX_THRESHOLD_2;
		}
		

		data[index + 0] = MSB_FIRST(level, littleEndian);
		data[index + 1] = MSB_FIRST(red, littleEndian);
		data[index + 2] = MSB_FIRST(green, littleEndian);
		data[index + 3] = MSB_FIRST(blue, littleEndian);
		index += 4;
	}

	return writeBytes(_ledBuffer.size(), _ledBuffer.data());
}

bool DriverSpiHD108::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("hd108", "leds_group_0_SPI", DriverSpiHD108::construct);
