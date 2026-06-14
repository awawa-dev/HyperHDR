#include <led-drivers/serial/DriverSerialSP616E.h>

// SP616E protocol:
//   - RGB pixels followed by a 0xFF terminator
//   - Frame size is dynamic: (_ledRGBCount) + 1 bytes
//   - No header, no checksum, no length field

static const uint8_t SP616E_TERMINATOR = 0xFF;

DriverSerialSP616E::DriverSerialSP616E(const QJsonObject& deviceConfig)
	: ProviderSerial(deviceConfig)
{
}

bool DriverSerialSP616E::init(QJsonObject deviceConfig)
{
	bool isInitOK = false;

	// Initialise sub-class
	if (ProviderSerial::init(deviceConfig))
	{

		_ledBuffer.resize(_ledRGBCount + 1, 0x00);
		_ledBuffer.back() = SP616E_TERMINATOR; // block-end byte

		isInitOK = true;
	}
	return isInitOK;
}

int DriverSerialSP616E::writeFiniteColors(const std::vector<ColorRgb>& ledValues)
{
	memcpy(_ledBuffer.data(), ledValues.data(), _ledRGBCount);
	return writeBytes(_ledBuffer.size(), _ledBuffer.data());
}

LedDevice* DriverSerialSP616E::construct(const QJsonObject& deviceConfig)
{
	return new DriverSerialSP616E(deviceConfig);
}

bool DriverSerialSP616E::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE(
	"sp616e", "leds_group_3_serial", DriverSerialSP616E::construct);
