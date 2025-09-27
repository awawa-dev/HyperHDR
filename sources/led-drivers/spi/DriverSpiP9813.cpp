#include <led-drivers/spi/DriverSpiP9813.h>

DriverSpiP9813::DriverSpiP9813(const QJsonObject& deviceConfig)
	: ProviderSpi(deviceConfig)
{
}

bool DriverSpiP9813::init(QJsonObject deviceConfig)
{
	bool isInitOK = false;

	// Initialise sub-class
	if (ProviderSpi::init(deviceConfig))
	{
		_ledBuffer.resize(_ledCount * 4 + 8, 0x00);
		isInitOK = true;
	}
	return isInitOK;
}

int DriverSpiP9813::writeFiniteColors(const std::vector<ColorRgb>& ledValues)
{
	if (_ledCount != ledValues.size())
	{
		Warning(_log, "P9813 led's number has changed (old: %d, new: %d). Rebuilding buffer.", _ledCount, ledValues.size());
		_ledCount = static_cast<uint>(ledValues.size());

		_ledBuffer.resize(0, 0x00);
		_ledBuffer.resize(_ledCount * 4 + 8, 0x00);
	}

	uint8_t* dataPtr = _ledBuffer.data();
	for (const ColorRgb& color : ledValues)
	{
		*dataPtr++ = calculateChecksum(color);
		*dataPtr++ = color.blue;
		*dataPtr++ = color.green;
		*dataPtr++ = color.red;
	}

	return writeBytes(static_cast<unsigned int>(_ledBuffer.size()), _ledBuffer.data());
}

uint8_t DriverSpiP9813::calculateChecksum(const ColorRgb& color) const
{
	uint8_t res = 0;

	res |= (uint8_t)0x03 << 6;
	res |= (uint8_t)(~(color.blue >> 6) & 0x03) << 4;
	res |= (uint8_t)(~(color.green >> 6) & 0x03) << 2;
	res |= (uint8_t)(~(color.red >> 6) & 0x03);

	return res;
}

LedDevice* DriverSpiP9813::construct(const QJsonObject& deviceConfig)
{
	return new DriverSpiP9813(deviceConfig);
}

bool DriverSpiP9813::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("p9813", "leds_group_0_SPI", DriverSpiP9813::construct);
