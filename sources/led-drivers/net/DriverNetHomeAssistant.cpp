#include <led-drivers/net/DriverNetHomeAssistant.h>

DriverNetHomeAssistant::DriverNetHomeAssistant(const QJsonObject& deviceConfig)
	: LedDevice(deviceConfig)
{
}

LedDevice* DriverNetHomeAssistant::construct(const QJsonObject& deviceConfig)
{
	return new DriverNetHomeAssistant(deviceConfig);
}

bool DriverNetHomeAssistant::init(const QJsonObject& deviceConfig)
{
	bool isInitOK = false;

	return isInitOK;
}

int DriverNetHomeAssistant::write(const std::vector<ColorRgb>& ledValues)
{
	return 0;
}

bool DriverNetHomeAssistant::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("home_assistant", "leds_group_2_network", DriverNetHomeAssistant::construct);
