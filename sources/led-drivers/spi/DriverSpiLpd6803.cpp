#include <led-drivers/spi/DriverSpiLpd6803.h>

///
/// Implementation of the LedDevice interface for writing to LDP6803 LED-device.
///
/// 00000000 00000000 00000000 00000000 1RRRRRGG GGGBBBBB 1RRRRRGG GGGBBBBB ...
/// |---------------------------------| |---------------| |---------------|
/// 32 zeros to start the frame LED1 LED2 ...
///
/// For each led, the first bit is always 1, and then you have 5 bits each for red, green and blue
/// (R, G and B in the above illustration) making 16 bits per led. Total bytes = 4 + (2 x number of LEDs)
///

DriverSpiLpd6803::DriverSpiLpd6803(const QJsonObject& deviceConfig)
	: ProviderSpi(deviceConfig)
{
}

LedDevice* DriverSpiLpd6803::construct(const QJsonObject& deviceConfig)
{
	return new DriverSpiLpd6803(deviceConfig);
}

bool DriverSpiLpd6803::init(const QJsonObject& deviceConfig)
{
	bool isInitOK = false;

	// Initialise sub-class
	if (ProviderSpi::init(deviceConfig))
	{
		unsigned messageLength = 4 + 2 * _ledCount + _ledCount / 8 + 1;
		// Initialise the buffer
		_ledBuffer.resize(messageLength, 0x00);

		isInitOK = true;
	}
	return isInitOK;
}

int DriverSpiLpd6803::write(const std::vector<ColorRgb>& ledValues)
{
	if (_ledCount != ledValues.size())
	{
		Warning(_log, "Lpd6803 led's number has changed (old: %d, new: %d). Rebuilding buffer.", _ledCount, ledValues.size());
		_ledCount = ledValues.size();

		_ledBuffer.resize(0, 0x00);

		unsigned messageLength = 4 + 2 * _ledCount + _ledCount / 8 + 1;
		// Initialise the buffer
		_ledBuffer.resize(messageLength, 0x00);
	}

	// Copy the colors from the ColorRgb vector to the Ldp6803 data vector
	for (unsigned iLed = 0; iLed < (unsigned)_ledCount; ++iLed)
	{
		const ColorRgb& color = ledValues[iLed];

		_ledBuffer[4 + 2 * iLed] = 0x80 | ((color.red & 0xf8) >> 1) | (color.green >> 6);
		_ledBuffer[5 + 2 * iLed] = ((color.green & 0x38) << 2) | (color.blue >> 3);
	}

	// Write the data
	return writeBytes(_ledBuffer.size(), _ledBuffer.data());
}

bool DriverSpiLpd6803::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("lpd6803", "leds_group_0_SPI", DriverSpiLpd6803::construct);
