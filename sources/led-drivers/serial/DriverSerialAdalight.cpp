/* DriverSerialAdalight.cpp
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

#include <led-drivers/serial/DriverSerialAdalight.h>
#include <infinite-color-engine/ColorSpace.h>
#include <QtEndian>

#include <cassert>

DriverSerialAdalight::DriverSerialAdalight(const QJsonObject& deviceConfig)
	: ProviderSerial(deviceConfig)
	, _headerSize(6)
	, _ligthBerryAPA102Mode(false)
	, _awa_mode(false)
	, _enable_ice_rgbw(false)
	, _ice_white_temperatur{ 1.0f, 1.0f, 1.0f }
	, _ice_white_mixer_threshold(0.0f)
	, _ice_white_led_intensity(1.8f)
	, _white_channel_calibration(false)
	, _white_channel_limit(255)
	, _white_channel_red(255)
	, _white_channel_green(255)
	, _white_channel_blue(255)
{
}

bool DriverSerialAdalight::init(QJsonObject deviceConfig)
{
	bool isInitOK = false;

	// Initialise sub-class
	if (ProviderSerial::init(deviceConfig))
	{

		_ligthBerryAPA102Mode = deviceConfig["lightberry_apa102_mode"].toBool(false);
		_awa_mode = deviceConfig["awa_mode"].toBool(false);

		_enable_ice_rgbw = deviceConfig["enable_ice_rgbw"].toBool(false);
		_ice_white_mixer_threshold = deviceConfig["ice_white_mixer_threshold"].toDouble(0.0);
		_ice_white_led_intensity = deviceConfig["ice_white_led_intensity"].toDouble(1.8);
		_ice_white_temperatur.x = deviceConfig["ice_white_temperatur_red"].toDouble(1.0);
		_ice_white_temperatur.y = deviceConfig["ice_white_temperatur_green"].toDouble(1.0);
		_ice_white_temperatur.z = deviceConfig["ice_white_temperatur_blue"].toDouble(1.0);
		Debug(_log, "Infinite Color Engine RGBW is: {:s}, white channel temp for the white LED: {:s}, white mixer threshold: {:f}, white LED intensity: {:f}",
				((_enable_ice_rgbw) ? "enabled" : "disabled"), ColorSpaceMath::vecToString(_ice_white_temperatur), _ice_white_mixer_threshold, _ice_white_led_intensity);

		_white_channel_calibration = deviceConfig["white_channel_calibration"].toBool(false);
		_white_channel_limit = qMin(qRound(deviceConfig["white_channel_limit"].toDouble(1) * 255.0 / 100.0), 255);
		_white_channel_red = qMin(deviceConfig["white_channel_red"].toInt(255), 255);
		_white_channel_green = qMin(deviceConfig["white_channel_green"].toInt(255), 255);
		_white_channel_blue = qMin(deviceConfig["white_channel_blue"].toInt(255), 255);

		CreateHeader();

		if (_white_channel_calibration && _awa_mode)
			Debug(_log, "White channel limit: {:d}, red: {:d}, green: {:d}, blue: {:d}", _white_channel_limit, _white_channel_red, _white_channel_green, _white_channel_blue);

		isInitOK = true;
	}
	return isInitOK;
}

void DriverSerialAdalight::CreateHeader()
{
	// create ledBuffer
	unsigned int totalLedCount = _ledCount;

	if (_ligthBerryAPA102Mode && !_awa_mode)
	{
		const unsigned int startFrameSize = 4;
		const unsigned int bytesPerRGBLed = 4;
		const unsigned int endFrameSize = qMax<unsigned int>(((_ledCount + 15) / 16), bytesPerRGBLed);
		_ledBuffer.resize((uint64_t)_headerSize + (((uint64_t)_ledCount) * bytesPerRGBLed) + startFrameSize + endFrameSize, 0x00);

		// init constant data values
		for (uint64_t iLed = 1; iLed <= static_cast<uint64_t>(_ledCount); iLed++)
		{
			_ledBuffer[iLed * 4 + _headerSize] = 0xFF;
		}
		Debug(_log, "Adalight driver with activated LightBerry APA102 mode");
	}
	else
	{
		_ligthBerryAPA102Mode = false;
		totalLedCount -= 1;
		auto finalSize = (uint64_t)_headerSize + _ledRGBCount + ((_awa_mode) ? 8 : 0);
		_ledBuffer.resize(finalSize, 0x00);

		if (_awa_mode)
			Debug(_log, "Adalight driver with activated high speeed & data integration check AWA protocol");
	}

	_ledBuffer[0] = 'A';
	_ledBuffer[1] = (_awa_mode) ? ((_enable_ice_rgbw) ? 'W' : 'w') : 'd';
	_ledBuffer[2] = (_awa_mode && (_white_channel_calibration && !_enable_ice_rgbw)) ? 'A' : 'a';
	qToBigEndian<quint16>(static_cast<quint16>(totalLedCount), &_ledBuffer[3]);
	_ledBuffer[5] = _ledBuffer[3] ^ _ledBuffer[4] ^ 0x55; // Checksum

	Debug(_log, "Adalight header for {:d} {:s} leds: {:c} {:c} {:c} {:d} {:d} {:d}", _ledCount, ((_enable_ice_rgbw) ? "infinite" : "finite"),
		_ledBuffer[0], _ledBuffer[1], _ledBuffer[2], _ledBuffer[3], _ledBuffer[4], _ledBuffer[5]);
}

std::pair<bool, int> DriverSerialAdalight::writeInfiniteColors(SharedOutputColors nonlinearRgbColors)
{
	if (nonlinearRgbColors->empty() || !_enable_ice_rgbw)
	{
		return { _enable_ice_rgbw, 0 };
	}

	if (_ledCount != nonlinearRgbColors->size())
	{
		Warning(_log, "Adalight infinite led count has changed (old: {:d}, new: {:d}). Rebuilding header.", _ledCount, nonlinearRgbColors->size());

		_ledCount = static_cast<uint>(nonlinearRgbColors->size());

		CreateHeader();
	}

	// RGBW by Infinite Color Engine
	_infiniteColorEngineRgbw.renderRgbwFrame(*nonlinearRgbColors, _currentInterval, _ice_white_mixer_threshold, _ice_white_led_intensity, _ice_white_temperatur, _ledBuffer, _headerSize, _colorOrder);

	// add space at the end for fletcher checksum
	auto wanted = _headerSize + _ledCount * sizeof(linalg::aliases::byte4) + 8;
	if (_ledBuffer.size() < wanted)
		_ledBuffer.resize(wanted);

	uint8_t* hasher = _ledBuffer.data() + _headerSize;
	uint8_t* writer = hasher + _ledCount * sizeof(linalg::aliases::byte4);

	uint16_t fletcher1 = 0, fletcher2 = 0, fletcherExt = 0;
	uint8_t position = 0;
	while (hasher < writer)
	{
		fletcherExt = (fletcherExt + (*(hasher) ^ (position++))) % 255;
		fletcher1 = (fletcher1 + *(hasher++)) % 255;
		fletcher2 = (fletcher2 + fletcher1) % 255;
	}
	*(writer++) = (uint8_t)fletcher1;
	*(writer++) = (uint8_t)fletcher2;
	*(writer++) = (uint8_t)((fletcherExt != 0x41) ? fletcherExt : 0xaa);
	auto bufferLength = writer - _ledBuffer.data();

	return  { true, writeBytes(static_cast<unsigned int>(bufferLength), _ledBuffer.data()) };
}

int DriverSerialAdalight::writeFiniteColors(const std::vector<ColorRgb>& ledValues)
{
	if (_ledCount != ledValues.size())
	{
		Warning(_log, "Adalight led count has changed (old: {:d}, new: {:d}). Rebuilding header.", _ledCount, ledValues.size());
		_ledCount = (uint)ledValues.size();
		_ledRGBCount = _ledCount * 3;
		CreateHeader();
	}

	if (_ligthBerryAPA102Mode)
	{
		for (uint64_t iLed = 1; iLed <= static_cast<uint64_t>(_ledCount); iLed++)
		{
			const ColorRgb& rgb = ledValues[iLed - 1];
			_ledBuffer[iLed * 4 + 7] = rgb.red;
			_ledBuffer[iLed * 4 + 8] = rgb.green;
			_ledBuffer[iLed * 4 + 9] = rgb.blue;
		}

		return writeBytes(_ledBuffer.size(), _ledBuffer.data());
	}
	else
	{
		auto bufferLength = _headerSize + ledValues.size() * sizeof(ColorRgb) + ((_awa_mode) ? 8 : 0);

		if (bufferLength > _ledBuffer.size())
		{
			Warning(_log, "Adalight buffer's size has changed. Skipping refresh.");
			return 0;
		}

		uint8_t* writer = _ledBuffer.data() + _headerSize;
		uint8_t* hasher = writer;

		memcpy(writer, ledValues.data(), ledValues.size() * sizeof(ColorRgb));
		writer += ledValues.size() * sizeof(ColorRgb);

		if (_awa_mode)
		{
			whiteChannelExtension(writer);

			uint16_t fletcher1 = 0, fletcher2 = 0, fletcherExt = 0;
			uint8_t position = 0;
			while (hasher < writer)
			{
				fletcherExt = (fletcherExt + (*(hasher) ^ (position++))) % 255;
				fletcher1 = (fletcher1 + *(hasher++)) % 255;
				fletcher2 = (fletcher2 + fletcher1) % 255;
			}
			*(writer++) = (uint8_t)fletcher1;
			*(writer++) = (uint8_t)fletcher2;
			*(writer++) = (uint8_t)((fletcherExt != 0x41) ? fletcherExt : 0xaa);
		}
		bufferLength = writer - _ledBuffer.data();

		return writeBytes(bufferLength, _ledBuffer.data());
	}
}

void DriverSerialAdalight::whiteChannelExtension(uint8_t*& writer)
{
	if (_awa_mode && _white_channel_calibration)
	{
		*(writer++) = _white_channel_limit;
		*(writer++) = _white_channel_red;
		*(writer++) = _white_channel_green;
		*(writer++) = _white_channel_blue;
	}
}

LedDevice* DriverSerialAdalight::construct(const QJsonObject& deviceConfig)
{
	return new DriverSerialAdalight(deviceConfig);
}

bool DriverSerialAdalight::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("adalight", "leds_group_3_serial", DriverSerialAdalight::construct);
