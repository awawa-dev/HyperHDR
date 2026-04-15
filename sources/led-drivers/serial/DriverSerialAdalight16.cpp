#include <led-drivers/serial/DriverSerialAdalight16.h>
#include <infinite-color-engine/ColorSpace.h>
#include <QtEndian>
#include <linalg.h>

#include <cassert>
#include <algorithm>
#include <cmath>

namespace
{
		inline uint16_t floatToQ16(const float value)
		{
			const float clamped = std::clamp(value, 0.0f, 1.0f);
			return static_cast<uint16_t>(std::lround(clamped * 65535.0f));
		}

	inline uint16_t addMod255Fast(uint16_t acc, uint16_t value)
	{
		acc = static_cast<uint16_t>(acc + value);
		if (acc >= 255u)
			acc = static_cast<uint16_t>(acc - 255u);
		if (acc >= 255u)
			acc = static_cast<uint16_t>(acc - 255u);
		return acc;
	}
}

std::pair<bool, int> DriverSerialAdalight16::writeInfiniteColors(SharedOutputColors nonlinearRgbColors)
{
	if (nonlinearRgbColors == nullptr || nonlinearRgbColors->empty() || !_enable_ice_rgbw)
		return { _enable_ice_rgbw, 0 };

	if (_ledCount != nonlinearRgbColors->size())
	{
		Warning(_log, "Adalight16 led count has changed (old: {:d}, new: {:d}). Rebuilding header.", _ledCount, nonlinearRgbColors->size());
		_ledCount = static_cast<uint>(nonlinearRgbColors->size());
		_ledRGBCount = _ledCount * 3;
		createHeader();
	}

	const uint64_t rgbwDataLength = static_cast<uint64_t>(nonlinearRgbColors->size()) * sizeof(linalg::aliases::byte4);
	const uint64_t bufferLength = static_cast<uint64_t>(_headerSize) + rgbwDataLength + 3u;

	if (bufferLength > _ledBuffer.size())
	{
		Warning(_log, "Adalight16 buffer size mismatch. Rebuilding header.");
		createHeader();
		if (bufferLength > _ledBuffer.size())
			return { true, -1 };
	}

	uint8_t* writer = _ledBuffer.data() + _headerSize;
	uint8_t* hasher = writer;
	_infiniteColorEngineRgbw.renderRgbwFrame(*nonlinearRgbColors, _currentInterval, _ice_white_mixer_threshold, _ice_white_led_intensity, _ice_white_temperatur, _ledBuffer, _headerSize, _colorOrder);
	writer += rgbwDataLength;

	uint16_t fletcher1 = 0;
	uint16_t fletcher2 = 0;
	uint16_t fletcherExt = 0;
	uint8_t position = 0;

	while (hasher < writer)
	{
		const uint8_t byte = *(hasher++);
		fletcherExt = addMod255Fast(fletcherExt, static_cast<uint16_t>(byte ^ (position++)));
		fletcher1 = addMod255Fast(fletcher1, byte);
		fletcher2 = addMod255Fast(fletcher2, fletcher1);
	}

	*(writer++) = static_cast<uint8_t>(fletcher1);
	*(writer++) = static_cast<uint8_t>(fletcher2);
	*(writer++) = static_cast<uint8_t>((fletcherExt != 0x41) ? fletcherExt : 0xaa);

	const int outLength = static_cast<int>(writer - _ledBuffer.data());
	return { true, writeBytes(outLength, _ledBuffer.data()) };
}

DriverSerialAdalight16::DriverSerialAdalight16(const QJsonObject& deviceConfig)
	: ProviderSerial(deviceConfig)
	, _headerSize(6)
	, _awa_mode(false)
	, _enable_ice_rgbw(false)
	, _ice_white_temperatur{ 1.0f, 1.0f, 1.0f }
	, _ice_white_mixer_threshold(0.0f)
	, _ice_white_led_intensity(1.8f)
{
	_white_channel_calibration = false;
	_white_channel_limit = 255;
	_white_channel_red = 255;
	_white_channel_green = 255;
	_white_channel_blue = 255;
}

bool DriverSerialAdalight16::init(QJsonObject deviceConfig)
{
	bool isInitOK = false;

	// Initialise sub-class
	if (ProviderSerial::init(deviceConfig))
	{
		_awa_mode = deviceConfig["awa_mode"].toBool(true);
		if (!_awa_mode)
		{
			Warning(_log, "Adalight16 requires AWA mode. Forcing awa_mode=true.");
			_awa_mode = true;
		}

		_enable_ice_rgbw = deviceConfig["enable_ice_rgbw"].toBool(false);
		_ice_white_mixer_threshold = deviceConfig["ice_white_mixer_threshold"].toDouble(0.0);
		_ice_white_led_intensity = deviceConfig["ice_white_led_intensity"].toDouble(1.8);
		_ice_white_temperatur.x = deviceConfig.contains("ice_white_temperatur_red") ? deviceConfig["ice_white_temperatur_red"].toDouble(1.0) : deviceConfig["ice_white_temperatur_r"].toDouble(1.0);
		_ice_white_temperatur.y = deviceConfig.contains("ice_white_temperatur_green") ? deviceConfig["ice_white_temperatur_green"].toDouble(1.0) : deviceConfig["ice_white_temperatur_g"].toDouble(1.0);
		_ice_white_temperatur.z = deviceConfig.contains("ice_white_temperatur_blue") ? deviceConfig["ice_white_temperatur_blue"].toDouble(1.0) : deviceConfig["ice_white_temperatur_b"].toDouble(1.0);
		Debug(_log, "Infinite Color Engine RGBW is: {:s}, white channel temp for the white LED: {:s}, white mixer threshold: {:f}, white LED intensity: {:f}",
			((_enable_ice_rgbw) ? "enabled" : "disabled"), ColorSpaceMath::vecToString(_ice_white_temperatur), _ice_white_mixer_threshold, _ice_white_led_intensity);

		_white_channel_calibration = deviceConfig["white_channel_calibration"].toBool(false);
		_white_channel_limit = qMin(qRound(deviceConfig["white_channel_limit"].toDouble(1) * 255.0 / 100.0), 255);
		_white_channel_red = qMin(deviceConfig["white_channel_red"].toInt(255), 255);
		_white_channel_green = qMin(deviceConfig["white_channel_green"].toInt(255), 255);
		_white_channel_blue = qMin(deviceConfig["white_channel_blue"].toInt(255), 255);

		createHeader();

		if (_white_channel_calibration)
			Debug(_log, "White channel limit: {:d}, red: {:d}, green: {:d}, blue: {:d}", _white_channel_limit, _white_channel_red, _white_channel_green, _white_channel_blue);

		Debug(_log, "Adalight16 driver with activated 16-bit AWA payload mode");

		isInitOK = true;
	}
	return isInitOK;
}

void DriverSerialAdalight16::createHeader()
{
	unsigned int totalLedCount = _ledCount;
	if (totalLedCount > 0)
		totalLedCount -= 1;

	const uint64_t rgb16DataLength = static_cast<uint64_t>(_ledCount) * 6u;
	const uint64_t rgbwDataLength = static_cast<uint64_t>(_ledCount) * sizeof(linalg::aliases::byte4);
	const uint64_t trailerLength = (_white_channel_calibration ? 4u : 0u) + 3u;
	_ledBuffer.resize(static_cast<uint64_t>(_headerSize) + (_enable_ice_rgbw ? (rgbwDataLength + 3u) : (rgb16DataLength + trailerLength)), 0x00);

	_ledBuffer[0] = 'A';
	_ledBuffer[1] = _enable_ice_rgbw ? 'W' : 'w';
	_ledBuffer[2] = _enable_ice_rgbw ? 'a' : (_white_channel_calibration ? 'B' : 'b');
	qToBigEndian<quint16>(static_cast<quint16>(totalLedCount), &_ledBuffer[3]);
	_ledBuffer[5] = _ledBuffer[3] ^ _ledBuffer[4] ^ 0x55; // Checksum

	Debug(_log, "Adalight16 header for {:d} leds: {:c} {:c} {:c} {:d} {:d} {:d}", _ledCount,
		_ledBuffer[0], _ledBuffer[1], _ledBuffer[2], _ledBuffer[3], _ledBuffer[4], _ledBuffer[5]);
}

int DriverSerialAdalight16::writeFiniteColors(const std::vector<ColorRgb>& ledValues)
{
	if (_ledCount != ledValues.size())
	{
		Warning(_log, "Adalight16 led count has changed (old: {:d}, new: {:d}). Rebuilding header.", _ledCount, ledValues.size());
		_ledCount = (uint)ledValues.size();
		_ledRGBCount = _ledCount * 3;
		createHeader();
	}

	const uint64_t rgb16DataLength = static_cast<uint64_t>(ledValues.size()) * 6u;
	const uint64_t trailerLength = (_white_channel_calibration ? 4u : 0u) + 3u;
	const uint64_t bufferLength = static_cast<uint64_t>(_headerSize) + rgb16DataLength + trailerLength;

	if (bufferLength > _ledBuffer.size())
	{
		Warning(_log, "Adalight16 buffer size mismatch. Rebuilding header.");
		createHeader();
		if (bufferLength > _ledBuffer.size())
			return 0;
	}

	uint8_t* writer = _ledBuffer.data() + _headerSize;
	uint8_t* hasher = writer;

	for (const ColorRgb& rgb : ledValues)
	{
		writeColorAs16Bit(writer, rgb);
	}

	whiteChannelExtension(writer);

	uint16_t fletcher1 = 0;
	uint16_t fletcher2 = 0;
	uint16_t fletcherExt = 0;
	uint8_t position = 0;

	while (hasher < writer)
	{
		const uint8_t byte = *(hasher++);
		fletcherExt = addMod255Fast(fletcherExt, static_cast<uint16_t>(byte ^ (position++)));
		fletcher1 = addMod255Fast(fletcher1, byte);
		fletcher2 = addMod255Fast(fletcher2, fletcher1);
	}

	*(writer++) = static_cast<uint8_t>(fletcher1);
	*(writer++) = static_cast<uint8_t>(fletcher2);
	*(writer++) = static_cast<uint8_t>((fletcherExt != 0x41) ? fletcherExt : 0xaa);

	const int outLength = static_cast<int>(writer - _ledBuffer.data());
	return writeBytes(outLength, _ledBuffer.data());
}

void DriverSerialAdalight16::whiteChannelExtension(uint8_t*& writer) const
{
	if (_white_channel_calibration)
	{
		*(writer++) = _white_channel_limit;
		*(writer++) = _white_channel_red;
		*(writer++) = _white_channel_green;
		*(writer++) = _white_channel_blue;
	}
}

void DriverSerialAdalight16::writeColorAs16Bit(uint8_t*& writer, const ColorRgb& rgb) const
{
	const uint16_t red = (static_cast<uint16_t>(rgb.red) << 8) | rgb.red;
	const uint16_t green = (static_cast<uint16_t>(rgb.green) << 8) | rgb.green;
	const uint16_t blue = (static_cast<uint16_t>(rgb.blue) << 8) | rgb.blue;

	*(writer++) = static_cast<uint8_t>(red >> 8);
	*(writer++) = static_cast<uint8_t>(red & 0xFF);
	*(writer++) = static_cast<uint8_t>(green >> 8);
	*(writer++) = static_cast<uint8_t>(green & 0xFF);
	*(writer++) = static_cast<uint8_t>(blue >> 8);
	*(writer++) = static_cast<uint8_t>(blue & 0xFF);
}

void DriverSerialAdalight16::writeColorAs16Bit(uint8_t*& writer, const linalg::aliases::float3& rgb) const
{
	const uint16_t red = floatToQ16(rgb.x);
	const uint16_t green = floatToQ16(rgb.y);
	const uint16_t blue = floatToQ16(rgb.z);

	*(writer++) = static_cast<uint8_t>(red >> 8);
	*(writer++) = static_cast<uint8_t>(red & 0xFF);
	*(writer++) = static_cast<uint8_t>(green >> 8);
	*(writer++) = static_cast<uint8_t>(green & 0xFF);
	*(writer++) = static_cast<uint8_t>(blue >> 8);
	*(writer++) = static_cast<uint8_t>(blue & 0xFF);
}

LedDevice* DriverSerialAdalight16::construct(const QJsonObject& deviceConfig)
{
	return new DriverSerialAdalight16(deviceConfig);
}

bool DriverSerialAdalight16::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("adalight16", "leds_group_3_serial", DriverSerialAdalight16::construct);
