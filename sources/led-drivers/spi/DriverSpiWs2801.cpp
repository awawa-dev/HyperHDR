#include <led-drivers/spi/DriverSpiWs2801.h>

DriverSpiWs2801::DriverSpiWs2801(const QJsonObject& deviceConfig)
	: ProviderSpi(deviceConfig)
{
}

LedDevice* DriverSpiWs2801::construct(const QJsonObject& deviceConfig)
{
	return new DriverSpiWs2801(deviceConfig);
}

bool DriverSpiWs2801::init(const QJsonObject& deviceConfig)
{
	bool isInitOK = ProviderSpi::init(deviceConfig);
	return isInitOK;
}

int DriverSpiWs2801::write(const std::vector<ColorRgb>& ledValues)
{
	const unsigned dataLen = _ledCount * sizeof(ColorRgb);
	const uint8_t* dataPtr = reinterpret_cast<const uint8_t*>(ledValues.data());

	return writeBytes(dataLen, dataPtr);
}

bool DriverSpiWs2801::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("ws2801", "leds_group_0_SPI", DriverSpiWs2801::construct);
