#include <led-drivers/net/DriverNetUdpRaw.h>

const ushort RAW_DEFAULT_PORT = 5568;

DriverNetUdpRaw::DriverNetUdpRaw(const QJsonObject& deviceConfig)
	: ProviderUdp(deviceConfig)
{
}

LedDevice* DriverNetUdpRaw::construct(const QJsonObject& deviceConfig)
{
	return new DriverNetUdpRaw(deviceConfig);
}

bool DriverNetUdpRaw::init(const QJsonObject& deviceConfig)
{
	_port = RAW_DEFAULT_PORT;

	// Initialise sub-class
	bool isInitOK = ProviderUdp::init(deviceConfig);
	return isInitOK;
}

int DriverNetUdpRaw::write(const std::vector<ColorRgb>& ledValues)
{
	const uint8_t* dataPtr = reinterpret_cast<const uint8_t*>(ledValues.data());

	if (ledValues.size() != _ledCount)
		setLedCount(static_cast<int>(ledValues.size()));

	return writeBytes(_ledRGBCount, dataPtr);
}

bool DriverNetUdpRaw::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("udpraw", "leds_group_2_network", DriverNetUdpRaw::construct);
