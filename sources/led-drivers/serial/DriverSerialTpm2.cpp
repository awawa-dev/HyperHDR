#include <led-drivers/serial/DriverSerialTpm2.h>


DriverSerialTpm2::DriverSerialTpm2(const QJsonObject& deviceConfig)
	: ProviderSerial(deviceConfig)
{
}

bool DriverSerialTpm2::init(QJsonObject deviceConfig)
{
	bool isInitOK = false;

	// Initialise sub-class
	if (ProviderSerial::init(deviceConfig))
	{

		_ledBuffer.resize(5 + _ledRGBCount);
		_ledBuffer[0] = 0xC9; // block-start byte
		_ledBuffer[1] = 0xDA; // DATA frame
		_ledBuffer[2] = (_ledRGBCount >> 8) & 0xFF; // frame size high byte
		_ledBuffer[3] = _ledRGBCount & 0xFF; // frame size low byte
		_ledBuffer.back() = 0x36; // block-end byte

		isInitOK = true;
	}
	return isInitOK;
}

int DriverSerialTpm2::writeFiniteColors(const std::vector<ColorRgb>& ledValues)
{
	memcpy(4 + _ledBuffer.data(), ledValues.data(), _ledRGBCount);
	return writeBytes(_ledBuffer.size(), _ledBuffer.data());
}

LedDevice* DriverSerialTpm2::construct(const QJsonObject& deviceConfig)
{
	return new DriverSerialTpm2(deviceConfig);
}

bool DriverSerialTpm2::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("tpm2", "leds_group_3_serial", DriverSerialTpm2::construct);
