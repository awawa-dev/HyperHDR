#include <led-drivers/spi/DriverSpiAPA102.h>

DriverSpiAPA102::DriverSpiAPA102(const QJsonObject& deviceConfig)
	: ProviderSpi(deviceConfig)
{
}

LedDevice* DriverSpiAPA102::construct(const QJsonObject& deviceConfig)
{
	return new DriverSpiAPA102(deviceConfig);
}

bool DriverSpiAPA102::init(const QJsonObject& deviceConfig)
{
	bool isInitOK = false;

	// Initialise sub-class
	if (ProviderSpi::init(deviceConfig))
	{
		createHeader();
		isInitOK = true;
	}
	return isInitOK;
}

void DriverSpiAPA102::createHeader()
{
	const unsigned int startFrameSize = 4;
	const unsigned int endFrameSize = qMax<unsigned int>(((_ledCount + 15) / 16), 4);
	const unsigned int APAbufferSize = (_ledCount * 4) + startFrameSize + endFrameSize;

	_ledBuffer.resize(0, 0xFF);
	_ledBuffer.resize(APAbufferSize, 0xFF);
	_ledBuffer[0] = 0x00;
	_ledBuffer[1] = 0x00;
	_ledBuffer[2] = 0x00;
	_ledBuffer[3] = 0x00;

	Debug(_log, "APA102 buffer created. Led's number: %d", _ledCount);
}

int DriverSpiAPA102::write(const std::vector<ColorRgb>& ledValues)
{
	if (_ledCount != ledValues.size())
	{
		Warning(_log, "APA102 led's number has changed (old: %d, new: %d). Rebuilding buffer.", _ledCount, ledValues.size());
		_ledCount = ledValues.size();

		createHeader();
	}

	for (signed iLed = 0; iLed < static_cast<int>(_ledCount); ++iLed) {
		const ColorRgb& rgb = ledValues[iLed];
		_ledBuffer[4 + iLed * 4] = 0xFF;
		_ledBuffer[4 + iLed * 4 + 1] = rgb.red;
		_ledBuffer[4 + iLed * 4 + 2] = rgb.green;
		_ledBuffer[4 + iLed * 4 + 3] = rgb.blue;
	}

	return writeBytes(_ledBuffer.size(), _ledBuffer.data());
}

bool DriverSpiAPA102::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("apa102", "leds_group_0_SPI", DriverSpiAPA102::construct);
