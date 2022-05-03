#include "LedDeviceAWA_spi.h"
#include <QtEndian>

LedDeviceAWA_spi::LedDeviceAWA_spi(const QJsonObject& deviceConfig)
	: ProviderSpi(deviceConfig)
{
	_headerSize = 6;

	_white_channel_calibration = false;
	_white_channel_limit = 255;
	_white_channel_red = 255;
	_white_channel_green = 255;
	_white_channel_blue = 255;
}

LedDevice* LedDeviceAWA_spi::construct(const QJsonObject& deviceConfig)
{
	return new LedDeviceAWA_spi(deviceConfig);
}

bool LedDeviceAWA_spi::init(const QJsonObject& deviceConfig)
{
	bool isInitOK = false;

	// Initialise sub-class
	if (ProviderSpi::init(deviceConfig))
	{
		_white_channel_calibration = deviceConfig["white_channel_calibration"].toBool(false);
		_white_channel_limit = qMin(qRound(deviceConfig["white_channel_limit"].toDouble(1) * 255.0 / 100.0), 255);
		_white_channel_red = qMin(deviceConfig["white_channel_red"].toInt(255), 255);
		_white_channel_green = qMin(deviceConfig["white_channel_green"].toInt(255), 255);
		_white_channel_blue = qMin(deviceConfig["white_channel_blue"].toInt(255), 255);

		CreateHeader();

		if (_white_channel_calibration)
			Debug(_log, "White channel limit: %i, red: %i, green: %i, blue: %i", _white_channel_limit, _white_channel_red, _white_channel_green, _white_channel_blue);

		isInitOK = true;
	}

	return isInitOK;
}

void LedDeviceAWA_spi::CreateHeader()
{
	unsigned int totalLedCount = _ledCount - 1;

	auto finalSize = (uint64_t)_headerSize + (_ledCount * 3) + 7;
	_ledBuffer.resize(finalSize, 0x00);

	Debug(_log, "SPI driver with data integration check AWA protocol");

	_ledBuffer[0] = 'A';
	_ledBuffer[1] = 'w';
	_ledBuffer[2] = (_white_channel_calibration) ? 'A' : 'a';
	qToBigEndian<quint16>(static_cast<quint16>(totalLedCount), &_ledBuffer[3]);
	_ledBuffer[5] = _ledBuffer[3] ^ _ledBuffer[4] ^ 0x55; // Checksum

	Debug(_log, "SPI header for %d leds: %c%c%c 0x%02x 0x%02x 0x%02x", _ledCount,
		_ledBuffer[0], _ledBuffer[1], _ledBuffer[2], _ledBuffer[3], _ledBuffer[4], _ledBuffer[5]);
}

int LedDeviceAWA_spi::write(const std::vector<ColorRgb>& ledValues)
{
	if (_ledCount != ledValues.size())
	{
		Warning(_log, "AWA spi led's number has changed (old: %d, new: %d). Rebuilding buffer.", _ledCount, ledValues.size());

		_ledCount = ledValues.size();

		CreateHeader();
	}

	auto bufferLength = _headerSize + ledValues.size() * sizeof(ColorRgb) + 7;

	if (bufferLength > _ledBuffer.size())
	{
		Warning(_log, "AWA SPI buffer's size has changed. Skipping refresh.");

		return 0;
	}

	uint8_t* writer = _ledBuffer.data() + _headerSize;
	uint8_t* hasher = writer;

	memcpy(writer, ledValues.data(), ledValues.size() * sizeof(ColorRgb));
	writer += ledValues.size() * sizeof(ColorRgb);

	whiteChannelExtension(writer);

	uint16_t fletcher1 = 0, fletcher2 = 0;
	while (hasher < writer)
	{
		fletcher1 = (fletcher1 + *(hasher++)) % 255;
		fletcher2 = (fletcher2 + fletcher1) % 255;
	}
	*(writer++) = (uint8_t)fletcher1;
	*(writer++) = (uint8_t)fletcher2;
	bufferLength = writer -_ledBuffer.data();

	if (_spiType == "esp8266")
		return writeBytesEsp8266(bufferLength, _ledBuffer.data());
	else if (_spiType == "esp32")
		return writeBytesEsp32(bufferLength, _ledBuffer.data());
	else
		return writeBytes(bufferLength, _ledBuffer.data());
}

void LedDeviceAWA_spi::whiteChannelExtension(uint8_t*& writer)
{
	if (_white_channel_calibration)
	{
		*(writer++) = _white_channel_limit;
		*(writer++) = _white_channel_red;
		*(writer++) = _white_channel_green;
		*(writer++) = _white_channel_blue;
	}
}
