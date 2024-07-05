#include <led-drivers/serial/DriverSerialSedu.h>

struct FrameSpec
{
	uint8_t id;
	size_t size;
};

DriverSerialSedu::DriverSerialSedu(const QJsonObject& deviceConfig)
	: ProviderSerial(deviceConfig)
{
}

LedDevice* DriverSerialSedu::construct(const QJsonObject& deviceConfig)
{
	return new DriverSerialSedu(deviceConfig);
}

bool DriverSerialSedu::init(const QJsonObject& deviceConfig)
{
	bool isInitOK = false;

	// Initialise sub-class
	if (ProviderSerial::init(deviceConfig))
	{
		std::vector<FrameSpec> frameSpecs{ {0xA1, 256}, {0xA2, 512}, {0xB0, 768}, {0xB1, 1536}, {0xB2, 3072} };

		for (const FrameSpec& frameSpec : frameSpecs)
		{
			if ((unsigned)_ledRGBCount <= frameSpec.size)
			{
				_ledBuffer.clear();
				_ledBuffer.resize(frameSpec.size + 3, 0);
				_ledBuffer[0] = 0x5A;
				_ledBuffer[1] = frameSpec.id;
				_ledBuffer.back() = 0xA5;
				break;
			}
		}

		if (_ledBuffer.empty())
		{
			//Warning(_log, "More rgb-channels required then available");
			QString errortext = "More rgb-channels required then available";
			this->setInError(errortext);
		}
		else
		{
			isInitOK = true;
		}
	}
	return isInitOK;
}

int DriverSerialSedu::write(const std::vector<ColorRgb>& ledValues)
{
	memcpy(_ledBuffer.data() + 2, ledValues.data(), ledValues.size() * sizeof(ColorRgb));
	return writeBytes(_ledBuffer.size(), _ledBuffer.data());
}

bool DriverSerialSedu::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("sedu", "leds_group_3_serial", DriverSerialSedu::construct);
