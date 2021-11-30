#include "LedDeviceAWA_spi.h"
#include <QtEndian>

LedDeviceAWA_spi::LedDeviceAWA_spi(const QJsonObject& deviceConfig)
	: ProviderSpi(deviceConfig)
{
	_headerSize = 6;
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
		CreateHeader();
		isInitOK = true;
	}

	return isInitOK;
}

void LedDeviceAWA_spi::CreateHeader()
{
	unsigned int totalLedCount = _ledCount - 1;

	_ledBuffer.resize((uint64_t)_headerSize + (_ledCount * 3) + 2, 0x00);

	Debug(_log, "SPI driver with data integration check AWA protocol");

	_ledBuffer[0] = 'A';
	_ledBuffer[1] = 'w';
	_ledBuffer[2] = 'a';
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

	if ((_headerSize + ledValues.size() * sizeof(ColorRgb) + 2) > _ledBuffer.size())
	{
		Warning(_log, "Adalight buffer's size has changed. Skipping refresh.");

		return 0;
	}

	memcpy(_headerSize + _ledBuffer.data(), ledValues.data(), ledValues.size() * sizeof(ColorRgb));

	uint16_t fletcher1 = 0, fletcher2 = 0;
	uint8_t* input = (uint8_t*)ledValues.data();
	for (uint16_t iLed = 0; iLed < ledValues.size() * sizeof(ColorRgb); iLed++, input++)
	{
		fletcher1 = (fletcher1 + *input) % 255;
		fletcher2 = (fletcher2 + fletcher1) % 255;
	}
	_ledBuffer[_headerSize + ledValues.size() * sizeof(ColorRgb)] = fletcher1;
	_ledBuffer[_headerSize + ledValues.size() * sizeof(ColorRgb) + 1] = fletcher2;

	if (_spiType == "esp8266")
		return writeBytesEsp8266(_ledBuffer.size(), _ledBuffer.data());
	else if (_spiType == "esp32")
		return writeBytesEsp32(_ledBuffer.size(), _ledBuffer.data());
	else
		return writeBytes(_ledBuffer.size(), _ledBuffer.data());
}
