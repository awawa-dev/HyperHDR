/* DriverNetDDP.cpp
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

#include <led-drivers/net/DriverNetDDP.h>
#include <infinite-color-engine/ColorSpace.h>

#include <QtEndian>

namespace {
	constexpr ushort DDP_DEFAULT_PORT = 4048;
}

DriverNetDDP::DriverNetDDP(const QJsonObject& deviceConfig)
	: ProviderUdp(deviceConfig)
	, _enable_ice_rgbw(false)
	, _ice_white_temperatur{ 1.0f, 1.0f, 1.0f }
	, _ice_white_mixer_threshold(0.0f)
	, _ice_white_led_intensity(1.8f)
	, _isRgbw(false)
	, _whiteAlgorithm(RGBW::WhiteAlgorithm::HYPERSERIAL_COLD_WHITE)
	, _white_channel_limit(255)
	, _white_channel_red(255)
	, _white_channel_green(255)
	, _white_channel_blue(255)
{
}

bool DriverNetDDP::init(QJsonObject deviceConfig)
{
	_port = DDP_DEFAULT_PORT;

	// Initialise sub-class
	bool isInitOK = false;

	// Initialise sub-class
	if (ProviderUdp::init(deviceConfig))
	{
		_enable_ice_rgbw = deviceConfig["enable_ice_rgbw"].toBool(false);
		_ice_white_mixer_threshold = deviceConfig["ice_white_mixer_threshold"].toDouble(0.0);
		_ice_white_led_intensity = deviceConfig["ice_white_led_intensity"].toDouble(1.8);
		_ice_white_temperatur.x = deviceConfig["ice_white_temperatur_red"].toDouble(1.0);
		_ice_white_temperatur.y = deviceConfig["ice_white_temperatur_green"].toDouble(1.0);
		_ice_white_temperatur.z = deviceConfig["ice_white_temperatur_blue"].toDouble(1.0);
		Debug(_log, "Infinite Color Engine RGBW is: {:s}, white channel temp for the white LED: {:s}, white mixer threshold: {:f}, white LED intensity: {:f}",
			((_enable_ice_rgbw) ? "enabled" : "disabled"), ColorSpaceMath::vecToString(_ice_white_temperatur), _ice_white_mixer_threshold, _ice_white_led_intensity);

		_isRgbw = deviceConfig["rgbw"].toBool(false);

		Debug(_log, "Type : {:s}", (_isRgbw) ? "RGBW" : "RGB");

		if (_isRgbw)
		{
			QString whiteAlgorithm = deviceConfig["white_algorithm"].toString("hyperserial_cold_white");

			_whiteAlgorithm = RGBW::stringToWhiteAlgorithm(whiteAlgorithm);
			_white_channel_limit = qMin(qRound(deviceConfig["white_channel_limit"].toDouble(1) * 255.0 / 100.0), 255);
			_white_channel_red = qMin(deviceConfig["white_channel_red"].toInt(255), 255);
			_white_channel_green = qMin(deviceConfig["white_channel_green"].toInt(255), 255);
			_white_channel_blue = qMin(deviceConfig["white_channel_blue"].toInt(255), 255);

			if (_whiteAlgorithm == RGBW::WhiteAlgorithm::INVALID)
			{
				Error(_log, "Unknown white channel algorithm: {:s}", whiteAlgorithm);
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

				isInitOK = true;
			}
		}
		else
		{
			isInitOK = true;
		}
	}
	return isInitOK;
}

int DriverNetDDP::writeFiniteColors(const std::vector<ColorRgb>& ledValues)
{
	if (ledValues.size() != _ledCount)
		setLedCount(static_cast<int>(ledValues.size()));

	return writeFiniteColors(_isRgbw, static_cast<int>(ledValues.size()), ledValues);
}

int DriverNetDDP::writeFiniteColors(bool isRgbw, const int ledsNumber, const std::vector<ColorRgb>& ledValues)
{
	static uint8_t sequenceNum = 0;

	if (isRgbw && ledValues.size() == 0 && static_cast<int>(_rgbwBuffer.size()) != ledsNumber * 4) {
		Error(_log, "RGBW colors were not provided");
		return 0;
	}

	// convert RGB to RGBW if neccesery
	if (isRgbw && ledValues.size() > 0) {
		_rgbwBuffer.resize(ledValues.size() * 4);
		for (auto rgbwBufferData = _rgbwBuffer.data(); const ColorRgb& color : ledValues)
		{
			ColorRgbw tempRgbw{ color.red, color.green, color.blue, 0 };
			RGBW::rgb2rgbw(color, &tempRgbw, _whiteAlgorithm, channelCorrection);
			memcpy(rgbwBufferData, &tempRgbw, sizeof(tempRgbw));
			rgbwBufferData += 4;
		}
	}

	const size_t colorSize = (isRgbw) ? 4 : 3;
	const size_t maxLedsInFrame = (isRgbw) ? 360 : 480;
	const uint8_t* start = (isRgbw) ? _rgbwBuffer.data() : reinterpret_cast<const uint8_t*>(ledValues.data());
	const uint8_t* end = start + ledsNumber * colorSize;
	uint32_t colorOffset = 0;
	constexpr size_t ddpHeaderOverhead = 10;
	const size_t maxColorsInUdpFrame = maxLedsInFrame * colorSize;

	size_t realRgbSize = std::min<size_t>(end - start, maxColorsInUdpFrame);
	
	_ddpFrame.reserve(realRgbSize + ddpHeaderOverhead);
	sequenceNum = (sequenceNum % 0x0F) + 1;

	while (start < end)
	{		
		realRgbSize = std::min<size_t>(end - start, maxColorsInUdpFrame);
		_ddpFrame.resize(realRgbSize + ddpHeaderOverhead, 0);

		bool isLast = (start + realRgbSize >= end);

		// Bajt 0: Flags (V1: 0x40 | Push: 0x01 = 0x41)
		_ddpFrame[0] = (isLast) ? 0x41 : 0x40;
		// Bajt 1: Sequence (0x00-0x0F)		
		_ddpFrame[1] = sequenceNum;
		// Bajt 2: Data Type (RGB = 0x0B, RGBW = 0x1B)
		_ddpFrame[2] = (isRgbw) ? 0x1B : 0x0B;
		// Bajt 3: Destination ID
		_ddpFrame[3] = 0x01;

		// offset in byte
		qToBigEndian<uint32_t>(colorOffset, &_ddpFrame[4]);

		// color data size in byte
		qToBigEndian<uint16_t>(static_cast<uint16_t>(realRgbSize), &_ddpFrame[8]);

		memcpy(_ddpFrame.data() + ddpHeaderOverhead, start, realRgbSize);
		start += realRgbSize;
		colorOffset += static_cast<uint32_t>(realRgbSize);

		// send UDP frame
		writeBytes(static_cast<int>(_ddpFrame.size()), _ddpFrame.data());
	}

	return static_cast<int>(ledsNumber);
}

std::pair<bool, int> DriverNetDDP::writeInfiniteColors(SharedOutputColors nonlinearRgbColors)
{
	if (nonlinearRgbColors->empty() || !_enable_ice_rgbw)
	{
		return { _enable_ice_rgbw, 0 };
	}
	
	_rgbwBuffer.resize(nonlinearRgbColors->size() * 4);
	_infiniteColorEngineRgbw.renderRgbwFrame(*nonlinearRgbColors, _currentInterval, _ice_white_mixer_threshold, _ice_white_led_intensity, _ice_white_temperatur, _rgbwBuffer, 0, _colorOrder);


	return { true, writeFiniteColors(true, static_cast<int>(nonlinearRgbColors->size()), {}) };
}

LedDevice* DriverNetDDP::construct(const QJsonObject& deviceConfig)
{
	return new DriverNetDDP(deviceConfig);
}

bool DriverNetDDP::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("ddp", "leds_group_2_network", DriverNetDDP::construct);
